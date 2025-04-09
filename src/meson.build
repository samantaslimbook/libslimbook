
libslimbook = shared_library('slimbook', ['slimbook.cpp','configuration.cpp','smbios.cpp', 'common.cpp', 'pci.cpp'], install: true, version: '1.0.0')

libpci     = dependency('libpci')
libudev = dependency('libudev')

executable('slimbookctl', ['slimbookctl.cpp'],
    link_with: libslimbook,
    install: true,
    install_mode: ['rwsr-xr-x','root','root'],
    dependencies : [libpci, libudev]
    )

install_headers('slimbook.h')
