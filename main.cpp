#include "utils.h"
#include "editor.h"

int main(int argc, char** argv) {
    enableRawMode();
    Editor e;
    if (argc >= 2) {
        e.openFile(argv[1]);
    }
    e.setStatusMessage("HELP: Ctrl-Q = quit");
    while (1) {
        e.refreshScreen();
        e.processKeyPress();
    }
    return 0;
}