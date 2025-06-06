/*
Copyright (C) 2023 Slimbook <dev@slimbook.es>

This file is part of libslimbook.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "slimbook.h"
#include "common.h"
#include "amdsmu.h"

#include "pci.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mntent.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <regex>
#include <string.h>

#define SLB_REPORT_PRIVATE "SLB_REPORT_PRIVATE"
#define SYS_AMDGPU "/sys/class/drm/card%d/device/"

using namespace std;

string generate_id()
{
    std::srand(std::time(nullptr));
    int rnd = std::rand();
    
    stringstream ss;
    
    ss<<std::hex<<std::setfill('0')<<std::setw(8)<<rnd;
    
    return ss.str();
}

static int run_command(string file, vector<string>args)
{
    pid_t pid = fork();
    
    if (pid == 0) {
        char* argv[10];
        int n = 1;

        argv[0] = (char*)file.c_str();

        while(n < (int)args.size() +1){
            argv[n] = (char*)args[n -1].c_str();n++;
        }

        argv[n] = nullptr;
        //switching to root UID
        setuid(0);
        int status = execvp(argv[0], argv);

        if (status < 0) {
            exit(status);
        }
        
        //this may not happen
        return 0;
    }
    else {
        int status;
        int ret = waitpid(pid,&status,0);
        
        if (ret > 0 and WIFEXITED(status)) {
            return WEXITSTATUS(status);
        }
        
        return 1;
    }
}

static string trim(string in)
{
    string out;
    size_t first = 0;
    size_t last = 0;
    bool ffound = false;
    
    for (size_t n=0;n<in.size();n++) {
        char c = in[n];
        
        if (!ffound and c!=' ') {
            ffound = true;
            first = n;
        }
        
        if (c!=' ') {
            last = n;
        }
    }
    out = in.substr(first,last+1);
    
    return out;
}

static string to_human(uint64_t value)
{
    string magnitude = "B";
    double tmp = value;
    
    if (tmp > 1024) {
        tmp /= 1024;
        magnitude = "KB";
    }
    
    if (tmp > 1024) {
        tmp /= 1024;
        magnitude = "MB";
    }
    
    if (tmp > 1024) {
        tmp /= 1024;
        magnitude = "GB";
    }
    
    stringstream ss;
    
    ss.precision(2);
    
    ss<<std::fixed<<tmp<<" "<<magnitude;
    
    return ss.str();
    
}

static string replace_ugly_chars(string in)
{
    stringstream ss;
    
    for (char c:in) {
        if (c > 32) {
            ss<<c;
        }
        else {
            ss<<"0x"<<std::hex<<(int)c;
        }
    }
    
    return ss.str();
}

void show_help()
{
    cout<<"Slimbook control tool"<<endl;
    cout<<"Usage: slimbookctl [command]"<<endl;
    cout<<"\n"<<endl;
    cout<<"Commands:"<<endl;
    cout<<"info: display Slimbook model information"<<endl;
    cout<<"get-kbd-backlight: shows current keyboard backlight value in 32bit hexadecimal"<<endl;
    cout<<"set-kbd-backlight HEX: sets keyboard backlight as 32bit hexadecimal"<<endl;
    cout<<"config-load: loads module settings"<<endl;
    cout<<"config-store: stores module settings to disk"<<endl;
    cout<<"report: creates a tar.gz with system information"<<endl;
    cout<<"report-full: same as report, but it also gathers some sensible data as MAC address or board serial number"<<endl;
    cout<<"help: show this help"<<endl;
}

string get_info()
{
    stringstream sout;
    
    map<int,string> yesno = {{0,"no"},{1,"yes"}};
    map<int,string> module_status_string = {{SLB_MODULE_NOT_LOADED,"no"},
                                            {SLB_MODULE_LOADED,"yes"},
                                            {SLB_MODULE_NOT_NEEDED,"not needed"},
                                            {SLB_MODULE_UNKNOWN,"unknown"}
                                            };
    
    int64_t uptime = slb_info_uptime();
    int64_t h = uptime / 3600;
    int64_t m = (uptime / 60) % 60;
    int64_t s = uptime % 60;
    
    sout<<"uptime: "<<h<<"h "<<m<<"m "<<s<<"s\n";
    sout<<"kernel: "<<slb_info_kernel()<<"\n";
    
    uint64_t tr,ar;
    
    tr = slb_info_total_memory();
    ar = slb_info_available_memory();
    
    sout<<"memory free/total: "<<to_human(ar)<<"/"<<to_human(tr)<<"\n";
    
    std::vector<string> mounts = {"/", "/home", "/boot/efi", "/boot"};
    
    FILE* mfile = setmntent("/proc/self/mounts","r");
    struct mntent* ent = getmntent(mfile);
    
    while(ent!=nullptr) {
        string dir = ent->mnt_dir;
        
        for (string& m : mounts) {
            
            if (dir == m) {
                struct statvfs stat;
        
                if (statvfs(dir.c_str(),&stat) == 0) {
                    uint64_t fbytes = stat.f_bsize * stat.f_bfree;
                    uint64_t tbytes = stat.f_bsize * stat.f_blocks;
                    
                    sout<<"disk free/total: "<<dir<<" "<<to_human(fbytes)<<"/"<<to_human(tbytes)<<endl;
                }
                
                break;
            }
        
        }
        //cout<<ent->mnt_fsname<<":"<<ent->mnt_dir<<endl;
        ent = getmntent(mfile);
    }
    
    // boot mode

    sout << (std::filesystem::exists("/sys/firmware/efi") ? "boot mode: UEFI\n" : "boot mode: legacy\n");
    
    sout<<"\n";
    
    sout<<"product: "<<slb_info_product_name()<<"\n";
    sout<<"sku: "<<slb_info_product_sku()<<"\n";
    sout<<"vendor: "<<slb_info_board_vendor()<<"\n";
    sout<<"bios: "<<slb_info_bios_version()<<"\n";
    sout<<"EC: "<<slb_info_ec_firmware_release()<<"\n";
    
    char* env = getenv(SLB_REPORT_PRIVATE);
    
    if (env and string(env) == "1") {
        sout<<"serial: ******\n";
    }
    else {
        sout<<"serial: "<<slb_info_product_serial()<<"\n";
    }

    sout<<"\n";
    
    slb_smbios_entry_t* entries = nullptr;
    int count = 0;

    if (slb_smbios_get(&entries,&count) == 0) {
        for (int n=0;n<count;n++) {
            if (entries[n].type == 4) {
                string name = trim(entries[n].data.processor.version);
                int count = entries[n].data.processor.threads;
                slb_tdp_info_t tdp = {0};
                 
                sout<<"cpu: "<<name<<" x "<<count<<"\n";

                tdp = slb_info_get_tdp_info();

                switch (tdp.type) {
                    case SLB_TDP_TYPE_INTEL:
                        sout << "TDP: "<< (int)tdp.sustained << " W\n";
                        break;

                    case SLB_TDP_TYPE_AMD:
                        sout << "TDP sustained (stapm): " << (int)tdp.sustained << " W\n";
                        sout << "TDP slow limit (ppt-s): " << (int)tdp.slow << " W\n";
                        sout << "TDP fast limit (ppt-l): " << (int)tdp.fast << " W\n";
                        break;

                    case SLB_TDP_TYPE_UNKNOWN:
                        /* do nothing */
                    default:
                        break;
                }

                sout << "\n";
            }

            if (entries[n].type == 17) {
                if (entries[n].data.memory_device.type > 2) {
                    sout<<"memory device: "<<entries[n].data.memory_device.size<< (entries[n].data.memory_device.size_unit == 0 ? " MB " : " KB ") << entries[n].data.memory_device.speed<<" MT/s"<<"\n";
                }
            }
        }
        
        slb_smbios_free(entries);
    }

    vector<string> modules = get_modules();
    bool modFound = false;

    for (string mod : modules) {
        modFound = mod == "amdgpu" ? true : false;

        if(modFound){
            break;
        }
    }

    if(modFound){
        
        string vram_val = "1";
        char buf[sizeof(SYS_AMDGPU)];
        
        for(int i = 0; i < 8; i++){
            snprintf(buf, sizeof(buf), SYS_AMDGPU, i);
            if(filesystem::exists(buf)){
                break;
            }
        }

        read_device(string(buf) + "mem_info_vram_total", vram_val);

        sout << "UMA Framebuffer: " << to_human(stoull(vram_val)) << "\n";
    }

    sout<<"\n";

    int ac_state;
    
    if (slb_info_get_ac_state(0, &ac_state) == 0) {
        string ac_state_text;
        
        switch (ac_state) {
            case 0:
                ac_state_text = "Offline";
            break;
            
            case 1:
                ac_state_text = "Online";
            break;
            
            case 2:
                ac_state_text = "Online Programmable";
            break;
            
            default:
                ac_state_text = "unknown";
        }
        
        sout<<"ac: "<<ac_state_text<<"\n";
    }
    
    slb_sys_battery_info bat = {0};

    if(slb_battery_info_get(&bat) == 0){
        string stat;

        switch(bat.status){
            case SLB_BAT_STATE_UNKNOWN:
                stat = "Unknown";
                break;
            case SLB_BAT_STATE_CHARGING:
                stat = "Charging";
                break;
            case SLB_BAT_STATE_DISCHARGING:
                stat = "Discharging";
                break;
            case SLB_BAT_STATE_NOT_CHARGING:
                stat = "Not charging";
                break;
            case SLB_BAT_STATE_FULL:
                stat = "Full";
                break;
            default:
                stat = "Unknown";
        }

        uint32_t charge = bat.charge;

        sout << "battery info: " << (int)(bat.capacity) << "% " << stat + " " << charge << " mAh" << "\n";
    }
    
    int module_status = slb_info_is_module_loaded();
    uint32_t platform = slb_info_get_platform();

    bool module_loaded = module_status == SLB_MODULE_LOADED;
    
    if(module_loaded){
        switch(platform){
            case SLB_PLATFORM_QC71:
                uint32_t fan1,fan2;

                slb_qc71_primary_fan_get(&fan1);
                slb_qc71_secondary_fan_get(&fan2);

                sout << "primary fan speed: " << fan1 << " RPM" << "\n";
                sout << "secondary fan speed: " << fan2 << " RPM" << "\n"; 

                break;

            case SLB_PLATFORM_CLEVO:
                //todo
                break;
        
            default:
                break;
        }
    }
    
    sout<<"\n";

    uint32_t model = slb_info_get_model();
    sout<<"model:0x"<<std::hex<<model<<"\n";
    
    sout<<"platform:0x"<<platform<<"\n";
    
    sout<<"family:"<<slb_info_get_family_name()<<"\n";
    
    int32_t confidence = slb_info_confidence();
    
    if (model != SLB_MODEL_UNKNOWN and confidence > 0) {
        sout<<"confidence:"<<std::dec<<confidence<<"\n";
    }
    
    sout<<"module loaded:"<<module_status_string[module_status]<<"\n";
    
    sout<<"\n";
    
    if (module_loaded and platform == SLB_PLATFORM_QC71) {
        uint32_t value = 0;
        
        slb_qc71_fn_lock_get(&value);
        sout<<"fn lock: "<<yesno[value]<<"\n";
        
        slb_qc71_super_lock_get(&value);
        sout<<"super key lock: "<<yesno[value]<<"\n";
        
        map<int,string> profile_gen_1 = {
            {SLB_QC71_PROFILE_SILENT,"silent"},
            {SLB_QC71_PROFILE_NORMAL,"normal"}
        };

        map<int,string> profile_gen_2 = {
            {SLB_QC71_PROFILE_SILENT,"silent"},
            {SLB_QC71_PROFILE_NORMAL,"normal"},
            {SLB_QC71_PROFILE_PERFORMANCE,"performance"}
        };

        map<int,string> profile_gen_3 = {
            {SLB_QC71_PROFILE_ENERGY_SAVER,"energy-saver"},
            {SLB_QC71_PROFILE_BALANCED,"balanced"},
            {SLB_QC71_PROFILE_PERFORMANCE,"performance"}
        };

        uint32_t profile = 0;
        string profile_name = "unknown";
        map<int, string> chosen_profile;

        switch (slb_info_get_family()) {
            case SLB_MODEL_PROX:
            case SLB_MODEL_EXECUTIVE:
                chosen_profile = profile_gen_1;
            break;

            case SLB_MODEL_TITAN:
            case SLB_MODEL_HERO:
                chosen_profile = profile_gen_2;
            break;

            case SLB_MODEL_EVO:
            case SLB_MODEL_CREATIVE:
                chosen_profile = profile_gen_3;
            break;
        }

        if (slb_qc71_profile_get(&profile) == 0) {
            profile_name = chosen_profile[profile];
        }

        sout<<"profile: "<<profile_name<<"\n";
    }
    
    sout<<std::flush;
    return sout.str();
}

void show_info()
{
    string info = get_info();
    cout<<info;
}

int main(int argc,char* argv[])
{
    string command;
    
    if (argc>1) {
        command = argv[1];
    }
    else {
        show_help();
        return 0;
    }
    
    if (command == "info") {
        show_info();
        return 0;
    }
    
    if (command == "help") {
        show_help();
        return 0;
    }
    
    if (command == "set-kbd-backlight") {
        if (argc<2) {
            return 1; //better return value
        }
        
        uint32_t value = std::stoi(argv[2],0,16);
        
        int status = slb_kbd_backlight_set(0,value);
        
        if (status > 0) {
            cerr<<"Failed to set keyboard backlight:"<<status<<endl;
            return status;
        }
        
        return 0;
    }
    
    if (command == "get-kbd-backlight") {
        uint32_t value;
        int status = slb_kbd_backlight_get(0,&value);
        
        if (status > 0) {
            cerr<<"Failed to retrieve keyboard backlight:"<<status<<endl;
            return status;
        }
        
        cout<<std::hex<<std::setw(6)<<std::setfill('0')<<value<<endl;
        
        return 0;
    }
    
    if (command == "config-load") {
        clog<<"loading slimbook configuration:";
        int status = slb_config_load(0);
        clog<<status<<endl;
    }

    if (command == "config-store") {
        clog<<"storing slimbook configuration:";
        int status = slb_config_store(0);
        clog<<status<<endl;
    }

    if (command == "serial") {
        cout<<slb_info_product_serial()<<"\n";
    }
    
    if (command == "report") {
        setenv(SLB_REPORT_PRIVATE,"1",1);
        command = "report-full";
    }
    
    if (command == "report-full") {
        
        string id = generate_id();
        string tmp_name = "/tmp/slimbook-report-" + id + "/";
        std::filesystem::create_directory(tmp_name);
        
        // store info
        string info = get_info();
        fstream finfo;
        finfo.open(tmp_name+"info.txt",fstream::out);
        
        if (finfo.good()) {
            finfo<<info;
            finfo.close();
        }
        // else throw error?
    
        for (const auto& entry : std::filesystem::directory_iterator("/usr/libexec/slimbook/report.d/")) {
            clog<<" running "<<entry.path().filename().string()<<" ";
            string output = tmp_name + entry.path().filename().string() + ".txt";
            int status = run_command(entry.path(),{output});
            
            if (status == 0) {
                clog<<"✓"<<endl;
            }
            else {
                if (status == 200) {
                    clog<<"⚑"<<endl;
                }
                else {
                    clog<<"✗"<<endl;
                }
            }
        }

        std::time_t now = std::time(NULL);
        std::tm time = *std::localtime(&now);
        stringstream stream;

        stream << std::put_time(&time, "%F-%H-%M-%S");
        string name = "slimbook-report-" + stream.str();

        run_command("/usr/libexec/slimbook/report-pack", {tmp_name, name}); 

        cout<<"report " << tmp_name << name << ".tar.gz" << endl; 
    }
    
    if (command == "show-dmi") {
        string product_name = slb_info_product_name();
        string product_sku = slb_info_product_sku();
        string vendor = slb_info_board_vendor();
        
        cout<<"product:["<<replace_ugly_chars(product_name)<<"]"<<endl;
        cout<<"sku:["<<replace_ugly_chars(product_sku)<<"]"<<endl;
        cout<<"vendor:["<<replace_ugly_chars(vendor)<<"]"<<endl;
        
    }
    
    return 0;
}
