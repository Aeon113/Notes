# Flash specific file systems
It benefits a lot that we expose the header data and leave the control of read/program/erase to the file system. For example, if the fs can mark the sectors of a deleted file obsolete, they won't be copied back and forth during GCs.   

