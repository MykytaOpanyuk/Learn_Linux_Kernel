cmd_/home/mopaniuk/qt_projects/test_module/hello.ko := ld -r -m elf_x86_64  -z max-page-size=0x200000 -T ./scripts/module-common.lds  --build-id  -o /home/mopaniuk/qt_projects/test_module/hello.ko /home/mopaniuk/qt_projects/test_module/hello.o /home/mopaniuk/qt_projects/test_module/hello.mod.o ;  true