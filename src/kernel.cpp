#include "common/types.h"
#include "gdt.h"
#include "multitasking.h"
#include "memorymanager.h"
#include "syscalls.h"
#include "shell.h"
#include "hardwarecommunication/interrupts.h"
#include "hardwarecommunication/pci.h"
#include "hardwarecommunication/times.h"
#include "drivers/driver.h"
#include "drivers/keyboard.h"
#include "drivers/mouse.h"
#include "drivers/vga.h"
#include "drivers/svga.h"
#include "gui/desktop.h"
#include "drivers/amd_am79c973.h"
#include "drivers/ata.h"
#include "filesystem/dospart.h"
#include "filesystem/fat.h"

using namespace myos;
using namespace myos::common;
using namespace myos::gui;
using namespace myos::drivers;
using namespace myos::filesystem;
using namespace myos::hardwarecommunication;

//#define GMODE1
#define GMODE2

void printf(const char* str){
    static uint16_t* VideoMemory = (uint16_t*)0xb8000;

    static uint8_t x = 0, y = 0;
    for(int i = 0; str[i]; i++){
        switch(str[i]) {
        case '\n':
            y++;
            x = 0;
            break;
        default:   
            VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0xFF00) | str[i];
            x++;
            break;
        }


        if(x >= 80) {
            x=0;
            y++;
        }

        if (y >=25) {
            for (y = 0; y < 25; y++) {
                for(x = 0; x < 80;x++) {
                    VideoMemory[80 * y + x] = (VideoMemory[80 * y + x] & 0xFF00) | ' ';
                }
            }
            x = 0, y = 0;
        }
    }
}

void printfHex(uint8_t key) {
    char* foo = (char*)"00";
    const char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0f];
    foo[1] = hex[key & 0x0f];
    printf((const char*)foo);
}

//delete!
const char* numToChar(uint8_t key) {
    char* foo = (char*)(new uint8_t* [2]);
    const char* hex = "0123456789ABCDEF";
    foo[0] = hex[(key >> 4) & 0x0f];
    foo[1] = hex[key & 0x0f];
    return (const char*)foo;
}

class PrintKeyboardEventHandler : public KeyBoardEventHandler {
public:
    void OnKeyDown(char c) {
        char* foo = (char*)" ";
        foo[0] = c;
        printf(foo);
    }
};

class MouseToConsole : public MouseEventHandler {
public:
    MouseToConsole()
        : x(40),
          y(12) {
     }

    void OnActivate() {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[y * 80 + x] = ((VideoMemory[y * 80 + x] & 0xf000) >> 4) |
                                ((VideoMemory[y * 80 + x] & 0x0f00) << 4) |
                                (VideoMemory[y * 80 + x] & 0x00ff);
    }
    
    void OnMouseMove(int8_t nx, int8_t ny) {
        uint16_t* VideoMemory = (uint16_t*)0xb8000;
        VideoMemory[y * 80 + x] = ((VideoMemory[y * 80 + x] & 0xf000) >> 4) |
                                ((VideoMemory[y * 80 + x] & 0xf00) << 4) |
                                (VideoMemory[y * 80 + x] & 0x00ff);

        x += nx;
        if(x < 0) x = 0;
        else if (x >= 80) x = 79;

        y += ny;
        if (y < 0) y = 0;
        else if (y >= 25) y = 24;

        VideoMemory[y * 80 + x] = ((VideoMemory[y * 80 + x] & 0xf000) >> 4) |
                                ((VideoMemory[y * 80 + x] & 0xf00) << 4) |
                                (VideoMemory[y * 80 + x] & 0x00ff);
    }
private:
    int8_t x, y;
};

void SysCallPrint(const char* str) {
    asm("int $0x80" : : "a" (4), "b" (str));
}

//more task
void TaskA() {
    while (1) {
        SysCallPrint((const char*)"A");
    }
}

void TaskB() {
    while (1) {
        SysCallPrint((const char*)"B");
    }
}

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void callConstructors(){
    for(constructor* i = &start_ctors; i != &end_ctors; i++){
        (*i)();
    }
}

extern "C" void kernelMain(multiboot_info_t* multiboot_structure, uint32_t magicnumber){
    GlobalDescriptorTable gdt;

    TaskManager taskmanager;
    Task task1(&gdt, TaskA);
    //Task task2(&gdt, TaskB);
    taskmanager.AddTask(&task1);
    //taskmanager.AddTask(&task2);

    printf("magicnumber: 0x");
    printfHex((magicnumber >> 24) & 0xff);
    printfHex((magicnumber >> 16) & 0xff);
    printfHex((magicnumber >> 8) & 0xff);
    printfHex((magicnumber >> 0) & 0xff);
    printf("   ");

    printf("vbe_mode: 0x");
    printfHex(((multiboot_structure->vbe_mode) >> 24) & 0xff);
    printfHex(((multiboot_structure->vbe_mode) >> 16) & 0xff);
    printfHex(((multiboot_structure->vbe_mode) >> 8) & 0xff);
    printfHex(((multiboot_structure->vbe_mode) >> 0) & 0xff);
    printf(" ");

    printf("vbe_mode_info->width: 0x");
    printfHex(((multiboot_structure->vbe_mode_info) >> 24) & 0xff);
    printfHex(((multiboot_structure->vbe_mode_info) >> 16) & 0xff);
    printfHex(((multiboot_structure->vbe_mode_info) >> 8) & 0xff);
    printfHex(((multiboot_structure->vbe_mode_info) >> 0) & 0xff);
    printf("\n");

    //mmanager
    size_t heap = 10 * 1024 * 1024;
    uint32_t* memupper = (uint32_t*)((size_t)multiboot_structure + 8);
    printf("memupper: 0x");
    printfHex(((*memupper) >> 24) & 0xff);
    printfHex(((*memupper) >> 16) & 0xff);
    printfHex(((*memupper) >> 8) & 0xff);
    printfHex(((*memupper) >> 0) & 0xff);
    printf("   ");

    MemoryManager memoryManager(heap, (*memupper) * 1024 - heap - 10 * 1024);

    printf("heap: 0x");
    printfHex((heap >> 24) & 0xff);
    printfHex((heap >> 16) & 0xff);
    printfHex((heap >> 8) & 0xff);
    printfHex((heap >> 0) & 0xff);

    printf("\n-----init1-----\n");

    InterruptManager interrupts(0x20, &gdt, &taskmanager);
    SysCallHandler syscalls(&interrupts, 0x80);

    #ifdef GMODE1
        Desktop desktop(320,200, 0x00,0x00,0xA8);
    #endif
    #ifdef GMODE2
        Desktop desktop(multiboot_structure->framebuffer_width, multiboot_structure->framebuffer_height, 0x87, 0xCE, 0xEB);
    #endif

    DriverManger drvManger;
    
    #ifdef GMODE1
        KeyBoardDriver keyboard(&interrupts, &desktop);
        MouseDriver mouse(&interrupts, &desktop);
    #else
        #ifdef GMODE2
            KeyBoardDriver keyboard(&interrupts, &desktop);
            MouseDriver mouse(&interrupts, &desktop);
        #else
            PrintKeyboardEventHandler kbhandler;
            MouseToConsole mousehandler;
            KeyBoardDriver keyboard(&interrupts, &kbhandler);
            MouseDriver mouse(&interrupts, &mousehandler);
        #endif
    #endif
    drvManger.AddDriver(&keyboard);
    drvManger.AddDriver(&mouse);
    
    PeripheralComponentInterconnectController PCIController;
    PCIController.SelectDrivers(&drvManger, &interrupts);

    Times timeControl(0x70, 0x71);

    #ifdef GMODE1
        VideoGraphicsArray vga;
    #endif
    #ifdef GMODE2
        uint32_t pixelwidth = multiboot_structure->framebuffer_bpp / 8;
        SuperVideoGraphicsArray svga((uint8_t*)multiboot_structure->framebuffer_addr, multiboot_structure->framebuffer_width, 
                                      multiboot_structure->framebuffer_height, multiboot_structure->framebuffer_pitch,
                                      multiboot_structure->framebuffer_bpp, pixelwidth);
    #endif

    drvManger.ActivateAll(); 

    Shell shell1;

    //amd_am79c973* eth0 = (amd_am79c973*)(drvManger.drivers[2]);
    //eth0->Send((uint8_t*)"Hello Network", 13);

    printf("\nS-ATA primary master: ");
    AdvancedTechnologyAttachment ata0m(true, 0x1F0);
    ata0m.Identify();
    
    printf("   S-ATA primary slave: ");
    AdvancedTechnologyAttachment ata0s(false, 0x1F0);
    ata0s.Identify();

    PartitionManger partManger;
    DOSPartitonTable::ReadPartitionTable(&ata0s, &partManger);
    FatPartition* part1 = partManger.GetPartitionList(1);
    DirectoriesFat32 part1Dirent[32] = {0};
    part1->GetFatFileList((DirectoriesFat32*)&part1Dirent);
    char* filename = part1->ReadFileName(part1Dirent[1]);
    shell1.ShellPrintf((const char*)filename);
    part1->ReadTxtFile(part1Dirent[1], &shell1);
    shell1.ShellPrintf((const char*)"\n");

    //AdvancedTechnologyAttachment ata1m(true, 0x170);
    //ata1m.Identify();
    //AdvancedTechnologyAttachment ata1s(false, 0x170);
    //ata1s.Identify();
    // third: 0x1E8
    // fourth: 0x168

    printf("\n");

    interrupts.Activate();

    printf("\n-----Init Done-----\n");
    
    #ifdef GMODE1
        vga.SetMode(320, 200, 8);
        Window win1(&desktop, 10, 10, 20, 20, 0xA8, 0x00, 0x00);
        desktop.AddChild(&win1);
        Window win2(&desktop, 40,15,30,30, 0x00,0xA8,0x00);
        desktop.AddChild(&win2);
    #endif
    #ifdef GMODE2
        CompositeWidget tool1(&desktop, 0, 0, multiboot_structure->framebuffer_width, 30, 128, 128, 128, 0, (uint8_t*)"tool");
        desktop.AddChild(&tool1);
        Widget timeStringMonth(&tool1, 10, 3, 330, 200, 0xFF, 0xFF, 0xFF, 2, (uint8_t*)new uint8_t[4]);
        Widget timeStringDate(&tool1, 35, 3, 330, 200, 0xFF, 0xFF, 0xFF, 2, (uint8_t*)new uint8_t[3]);
        Widget timeStringHour(&tool1, 60, 3, 330, 200, 0xFF, 0xFF, 0xFF, 2, (uint8_t*)new uint8_t[4]);
        Widget timeStringMin(&tool1, 90, 3, 330, 200, 0xFF, 0xFF, 0xFF, 2, (uint8_t*)new uint8_t[3]);
        tool1.AddChild(&timeStringMonth);
        tool1.AddChild(&timeStringDate);
        tool1.AddChild(&timeStringHour);
        tool1.AddChild(&timeStringMin);

        Window win1(&desktop, 114, 230, 350, 230, 0xFF, 0x00, 0x00, (uint8_t*)"win1");
        desktop.AddChild(&win1);
        Widget string1(&win1, 5, 25, 330, 200, 0xFF, 0xFF, 0xFF, 2, shell1.GetShellText());
        win1.AddChild(&string1);
        Window win2(&desktop, 568,230,200,100, 0x00,0xAA,0x00,(uint8_t*)"win2");
        desktop.AddChild(&win2);

        timeStringMonth.stringText[2] = '/';
        timeStringHour.stringText[2] = ':';
    #endif

    while(1) {
        
    svga.Redraw();
        desktop.Draw(&svga);
        
        timeControl.ReadRtc();
        timeStringMonth.stringText[0] = timeControl.month/10 + 48;
        timeStringMonth.stringText[1] = timeControl.month%10 + 48;

        timeStringDate.stringText[0] = timeControl.day/10 + 48;
        timeStringDate.stringText[1] = timeControl.day%10 + 48;

        timeStringHour.stringText[0] = timeControl.hour/10 + 48;
        timeStringHour.stringText[1] = timeControl.hour%10 + 48;

        timeStringMin.stringText[0] = timeControl.minute/10 + 48;
        timeStringMin.stringText[1] = timeControl.minute%10 + 48;
    }
}