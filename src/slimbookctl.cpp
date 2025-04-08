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

#include <cpuid.h>
extern "C"{
#include <pci/pci.h>
}
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <mntent.h>
#include <unistd.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <map>
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

/* Gets TDP from Zone 0 in CPU */
uint64_t _get_TDP_intel(){
    #define INTEL_RAPL_PATH "/sys/class/powercap/intel-rapl/intel-rapl:0/"
    uint64_t uw = 0;

    if(filesystem::exists(INTEL_RAPL_PATH)){
        string svalue;
        read_device(INTEL_RAPL_PATH"constraint_0_power_limit_uw", svalue);

        uw = atoll(svalue.c_str());
    }
    
    return uw;
};

typedef enum {
    DESIGN_UNKNOWN = -1,
    /* Family 0x17 */
    DESIGN_RAVEN,
    DESIGN_PICASSO,
    DESIGN_DALI,
    DESIGN_STARSHIP,
    DESIGN_RENOIR,
    DESIGN_LUCIENNE,
    DESIGN_MATISSE,
    DESIGN_VAN_GOGH,
    DESIGN_MERO,
    DESIGN_MENDOCINO,
    /* Family 0x19 */
    DESIGN_MILAN,
    DESIGN_CHAGALL,
    DESIGN_VERMEER,
    DESIGN_BADAMI,
    DESIGN_REMBRANDT,
    DESIGN_CEZANNE,
    DESIGN_STORM_PEAK,
    DESIGN_RAPHAEL,
    DESIGN_PHOENIX,
    DESIGN_PHOENIX_2,
    DESIGN_GENOA,
    /* Family 0x1A */
    DESIGN_TURIN,
    DESIGN_TURIN_DENSE,
    DESIGN_STRIX_POINT_1,
    DESIGN_STRIX_POINT_2,
    DESIGN_STRIX_HALO,
    DESIGN_GRANITE_RIDGE,
    DESIGN_FIRE_RANGE,
    DESIGN_KRACKAN_POINT_1,
    DESIGN_SARLAK,
}_amd_design;


/* Uses switch range C++ extension, not standard but impl in important compilers */
static void _get_design_amd(uint32_t family, uint32_t model, uint32_t* design){
    switch(family){
        case 0x17:
            switch(model){
                case 0x11:
                    *design = DESIGN_RAVEN;
                    return;
                case 0x18:
                    *design = DESIGN_PICASSO;
                    return;
                case 0x20:
                    *design = DESIGN_DALI;
                    return;
                case 0x31:
                    *design = DESIGN_STARSHIP;
                    return;
                case 0x47:
                case 0x87:
                    // figure out
                    return;
                case 0x60:
                    *design = DESIGN_RENOIR;
                    return;
                case 0x68:
                    *design = DESIGN_LUCIENNE;
                    return;
                case 0x71:
                    *design = DESIGN_MATISSE;
                    return;
                case 0x90:
                    *design = DESIGN_VAN_GOGH;
                    return;
                case 0x98:
                    *design = DESIGN_MERO;
                case 0xA0:
                    *design = DESIGN_MENDOCINO;
                    return;
                default:
                    *design = DESIGN_UNKNOWN;
                    return;
            }
            return;
        case 0x19:
            switch(model){
                case 0x1:
                    *design = DESIGN_MILAN;
                    return;
                case 0x8:
                    *design = DESIGN_CHAGALL;
                    return;
                case 0x20 ... 0x2F :
                    *design = DESIGN_VERMEER;
                    return;
                case 0x30 ... 0x3F:
                    *design = DESIGN_BADAMI;
                    return;
                case 0x40 ... 0x4F:
                    *design = DESIGN_REMBRANDT;
                    return;
                case 0x50 ... 0x5F:
                    *design = DESIGN_CEZANNE;
                    return;
                case 0x10 ... 0x1F:
                    *design = DESIGN_STORM_PEAK;
                    return;
                case 0x60 ... 0x6F:
                    *design = DESIGN_RAPHAEL;
                    return;
                case 0x70 ... 0x77:
                    *design = DESIGN_PHOENIX;
                    return;
                case 0x78 ... 0x7F:
                    *design = DESIGN_PHOENIX_2;
                    return;
                case 0xA0 ... 0xAF:
                    *design = DESIGN_GENOA;
                    return;
                default:
                    *design = DESIGN_UNKNOWN;
                    return;
            } 
            case 0x1A:
                switch(model){
                    case 0x0 ... 0xF:
                        *design = DESIGN_TURIN;
                        return;
                    case 0x10 ... 0x1F:
                        *design = DESIGN_TURIN_DENSE;
                        return;
                    case 0x20 ... 0x2F:
                        *design = DESIGN_STRIX_POINT_1;
                        return;
                    case 0x30 ... 0x37:
                        *design = DESIGN_STRIX_POINT_2;
                        return;
                    case 0x38 ... 0x3F:
                        *design = DESIGN_STRIX_HALO;
                        return;
                    case 0x40 ... 0x4F:
                        *design = DESIGN_GRANITE_RIDGE;
                        return;
                    case 0x50 ... 0x5F:
                        *design = DESIGN_FIRE_RANGE;
                        return;
                    case 0x60 ... 0x6F:
                        *design = DESIGN_KRACKAN_POINT_1;
                        return;
                    case 0x70 ... 0x77:
                        *design = DESIGN_SARLAK;
                        return;
                    default:
                        *design = DESIGN_UNKNOWN;
                        return;        
                }  

            /* TODO: Support older families than Zen? */
            default:
                *design = DESIGN_UNKNOWN;
                return;
    }   
}

#define cpuid(level, regs) do{ __cpuid((level), (regs)[0],(regs)[1],(regs)[2],(regs)[3]); }while(0)

typedef struct _smu_amd{
    struct pci_dev* dev;
    uint32_t msg;
    uint32_t res;
    uint32_t arg_base;
}smu_amd;

#define MSG_MSG_ADDR 0x3B10a20
#define MSG_RES_ADDR 0x3B10a80
#define MSG_ARG_BASE_ADDR 0x3B10a88

smu_amd* _get_smu_amd(){
    struct pci_access* o = pci_alloc();
    struct pci_dev* dev;
    smu_amd* smu;
    
    smu = (smu_amd*)((char*)malloc(sizeof(smu_amd)));

    pci_init(o);

    dev = pci_get_dev(o, 0,0,0,0);
    pci_fill_info(dev, PCI_FILL_IDENT|PCI_FILL_BASES|PCI_FILL_CLASS);


    smu->dev = dev;
    smu->msg = MSG_MSG_ADDR;
    smu->res = MSG_RES_ADDR;
    smu->arg_base = MSG_ARG_BASE_ADDR;
    
    return smu;
}

void _clear_smu_amd(smu_amd* smu){
    if(smu != nullptr){
        free(smu);
    }
}

#define DEV_PCI_REG_ADDR_ADDR 0xB8
#define DEV_PCI_REG_DATA_ADDR 0xBC

static uint32_t _pci_reg_rd(pci_dev* dev, uint32_t addr){
    pci_write_long(dev, DEV_PCI_REG_ADDR_ADDR, ALIGN(addr, 4));
    return pci_read_long(dev, DEV_PCI_REG_DATA_ADDR);
}

static void _pci_reg_wr(pci_dev* dev, uint32_t addr, uint32_t data){
    pci_write_long(dev, DEV_PCI_REG_ADDR_ADDR, addr);
    pci_write_long(dev, DEV_PCI_REG_DATA_ADDR, data);
}

#define MSG_ARG_ADDR(base, num) ((base) + 4 * (num))

uint32_t _smu_amd_send_req(smu_amd* smu, uint32_t msg, uint32_t* args){
    uint32_t res = 0;

    _pci_reg_wr(smu->dev, smu->res, 0);

    _pci_reg_wr(smu->dev, MSG_ARG_ADDR(smu->arg_base, 0), args[0]);
    _pci_reg_wr(smu->dev, MSG_ARG_ADDR(smu->arg_base, 1), args[1]);

    _pci_reg_wr(smu->dev, smu->msg, msg);

    while(res == 0){
        res = _pci_reg_rd(smu->dev, smu->res);
    }

    args[0] = _pci_reg_rd(smu->dev, MSG_ARG_ADDR(smu->arg_base, 0));
    args[1] = _pci_reg_rd(smu->dev, MSG_ARG_ADDR(smu->arg_base, 1));


    return res;
}

static void* phys_map = MAP_FAILED;

void _map_dev_addr(uintptr_t addr){
    int dev_fd = open("/dev/mem", O_RDONLY);

    if(dev_fd > 0){
        phys_map = mmap(NULL, 4096, PROT_READ, MAP_SHARED, dev_fd, addr);
        close(dev_fd);
    }
}

void _free_map_dev(){
    if(phys_map != MAP_FAILED){
        munmap((void*)phys_map, 4096);
    }
}

uint32_t _request_addr(uint32_t design, uintptr_t* addr, smu_amd** smu, uint32_t* smuargs){
    uint32_t table_msg = -1;

    switch(design){
        case DESIGN_RAVEN:
        case DESIGN_PICASSO:
        case DESIGN_DALI:
            smuargs[0] = 3;
            table_msg = 0xB;
            break;
        
        case DESIGN_RENOIR:
        case DESIGN_LUCIENNE:
        case DESIGN_CEZANNE:
        case DESIGN_REMBRANDT:
        case DESIGN_PHOENIX:
        case DESIGN_PHOENIX_2:
            table_msg = 0x66;
            break;
        default:
            return -1;
            break;
    }

    if(table_msg != (uint32_t)-1){
        uint32_t res;

        *smu = _get_smu_amd();
        res = _smu_amd_send_req(*smu, table_msg, smuargs);
        
        if(res != 1){
            return -1;
        }

        switch(design){
            case DESIGN_REMBRANDT:
            case DESIGN_PHOENIX:
            case DESIGN_PHOENIX_2:
                *addr = (uint64_t) smuargs[1] << 32 | smuargs[0];
                return 0;
            default:
                *addr = (uint64_t)smuargs[0];
                return 0;
        }
    }
    else{
        return -1;
    }

}

uint32_t _refresh_table(uint32_t design, smu_amd** smu, uint32_t* smuargs){
    uint32_t table_msg = -1;

    switch(design){
        case DESIGN_RAVEN:
        case DESIGN_PICASSO:
        case DESIGN_DALI:
            smuargs[0] = 3;
            table_msg = 0x3D;
            break;
        
        case DESIGN_RENOIR:
        case DESIGN_LUCIENNE:
        case DESIGN_CEZANNE:
        case DESIGN_REMBRANDT:
        case DESIGN_PHOENIX:
        case DESIGN_PHOENIX_2:
            table_msg = 0x65;
            break;
        default:
            return -1;
            break;
    }

    if(table_msg != (uint32_t)-1){
        uint32_t res;

        res = _smu_amd_send_req(*smu, table_msg, smuargs);

        if(res == 253){
            usleep(200 * 1000);
            res = _smu_amd_send_req(*smu, table_msg, smuargs);
        }

        if(res == 253){
            return -1;
        }
    }

    return 0;
}

float _get_TDP_amd(){
    float uw;

    uint32_t cpuregs[4];
    uint32_t smuargs[2] = {0};

    uint32_t family;
    uint32_t model;
    uint32_t design = 0;

    uintptr_t addr = -1;

    smu_amd* smu = NULL;

    cpuid(1, cpuregs);

    /* follows CPUID AMD spec standard, however, AMD CPUs after 2003 all have base family as 0xF */
    family = ((cpuregs[0] & 0xF00) >> 8) == 0xF ? ((cpuregs[0] & 0xF00) >> 0x8) + (( cpuregs[0] & 0xFF00000) >> 0x14) : (cpuregs[0] & 0xF00) >> 8;
    model = ((cpuregs[0] & 0xF00) >> 8) == 0xF ? ((cpuregs[0] & 0xF0000) >> 0xC ) | ((cpuregs[0] & 0xF0) >> 4): (cpuregs[0] & 0xF0) >> 4;

    _get_design_amd(family, model, &design);

    if(_request_addr(design, &addr, &smu, smuargs) == (uint32_t)-1){
        return 0.0f;
    }

    if(addr != (uint64_t)-1){
        _map_dev_addr(addr); 
        _refresh_table(design, &smu, smuargs);

        uw = *(float*)((uintptr_t)phys_map + 0x8);
    }

    _clear_smu_amd(smu);
    _free_map_dev();

    return uw;
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
    
    sout<<"uptime:"<<h<<"h "<<m<<"m "<<s<<"s\n";
    sout<<"kernel:"<<slb_info_kernel()<<"\n";
    
    uint64_t tr,ar;
    
    tr = slb_info_total_memory();
    ar = slb_info_available_memory();
    
    sout<<"memory free/total:"<<to_human(ar)<<"/"<<to_human(tr)<<"\n";
    
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
                    
                    sout<<"disk free/total:"<<dir<<" "<<to_human(fbytes)<<"/"<<to_human(tbytes)<<endl;
                }
                
                break;
            }
        
        }
        //cout<<ent->mnt_fsname<<":"<<ent->mnt_dir<<endl;
        ent = getmntent(mfile);
    }
    
    // boot mode

    sout << (std::filesystem::exists("/sys/firmware/efi") ? "boot mode: UEFI\n" : "boot mode: legacy\n");

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
    
    sout<<"\n";
    
    sout<<"product:"<<slb_info_product_name()<<"\n";
    sout<<"sku:"<<slb_info_product_sku()<<"\n";
    sout<<"vendor:"<<slb_info_board_vendor()<<"\n";
    sout<<"bios:"<<slb_info_bios_version()<<"\n";
    sout<<"EC:"<<slb_info_ec_firmware_release()<<"\n";
    
    char* env = getenv(SLB_REPORT_PRIVATE);
    
    if (env and string(env) == "1") {
        sout<<"serial: ******\n";
    }
    else {
        sout<<"serial:"<<slb_info_product_serial()<<"\n";
    }

    sout<<"\n";
    
    slb_smbios_entry_t* entries = nullptr;
    int count = 0;
    int cpu_type = 1;


    if (slb_smbios_get(&entries,&count) == 0) {
        for (int n=0;n<count;n++) {
            if (entries[n].type == 4) {
                string name = trim(entries[n].data.processor.version);
                int count = entries[n].data.processor.threads;
                uint32_t wUsage = 0;
                 
                sout<<"cpu:"<<name<<" x "<<count<<endl;

                switch(cpu_type){
                    case 0:
                        wUsage = _get_TDP_intel() / 1000000;
                        break;
                    case 1:
                        wUsage = _get_TDP_amd();
                        break;
                    default:
                        break;
                }

                sout << "TDP/PPT: " << wUsage << "W" << endl;


            }
            
            if (entries[n].type == 17) {
                if (entries[n].data.memory_device.type > 2) {
                    sout<<"memory device:"<<entries[n].data.memory_device.size<< (entries[n].data.memory_device.size_unit == 0 ? " MB " : " KB ") << entries[n].data.memory_device.speed<<" MT/s"<<endl;
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
        #define SYS_AMDGPU "/sys/class/drm/card%d/device/"
        string vram_val = "1";
        char buf[sizeof(SYS_AMDGPU)];

        for(int i = 0; i < 8; i++){
            snprintf(buf, sizeof(buf), SYS_AMDGPU, i);
            if(filesystem::exists(buf)){
                break;
            }
        }

        read_device(string(buf) + "mem_info_vram_total", vram_val);

        sout << "UMA Framebuffer: " << to_human(stoull(vram_val)) << endl;
    }

    sout<<"\n";

    slb_sys_battery_info bat = {0};

    if(slb_battery_info_get(&bat) == 0){
        string stat;

        switch(bat.status){
            case 0:
                stat = "Unknown";
                break;
            case 1:
                stat = "Charging";
                break;            
            case 2:
                stat = "Discharging";
                break;            
            case 3:
                stat = "Not charging";
                break;
            case 4:
                stat = "Full";
                break;
            default:
                stat = "Unknown";
        }

        uint32_t charge = bat.charge;

        sout << "battery info: " << (int)(bat.capacity) << "% " << stat + " " << charge << " mAh" << endl;
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

                sout << "primary fan speed: " << fan1 << " RPM" << endl;
                sout << "secondary fan speed: " << fan2 << " RPM" << endl; 

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
        sout<<"fn lock:"<<yesno[value]<<"\n";
        
        slb_qc71_super_lock_get(&value);
        sout<<"super key lock:"<<yesno[value]<<"\n";
        
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

        sout<<"profile:"<<profile_name<<"\n";
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
