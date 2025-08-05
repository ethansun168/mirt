#include "utils.h"
#include "editor.h"

int main(int argc, char** argv) {
    enableRawMode();
    Editor e;
    e.config();
    if (argc >= 2) {
        e.openFile(argv[1]);
    }
    e.setStatusMessage(":q to quit");
    e.appendIfBufferEmpty();

    while (1) {
        e.refreshScreen();
        e.processKeyPress();
    }
    return 0;
}