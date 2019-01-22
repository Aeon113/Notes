#ifndef MYMOUSE_H
#define MYMOUSE_H

struct mymouse {
    int fn; // Function number
    int x;
    int y;
};

#define MYMOUSE_LEFTCLICK 0
#define MYMOUSE_RIGHTCLICK 1
#define MYMOUSE_RELMOVE 2

#define MakeMyMouseLeftClick(ptr) (((ptr)->fn=MYMOUSE_LEFTCLICK),(ptr))
#define MakeMyMouseRightClick(ptr) (((ptr)->fn=MYMOUSE_RIGHTCLICK),(ptr))
#define MakeMyMouseRelMove(ptr,move_x,move_y) (((ptr)->fn=MYMOUSE_RELMOVE),((ptr)->x=(move_x)),((ptr)->y=(move_y)),(ptr))

#endif // MYMOUSE_H
