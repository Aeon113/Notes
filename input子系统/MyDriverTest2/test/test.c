#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

const char *cmd  = "400 500 0";

int main(int argc, char *argv[]) {
	int fd = 0;
    fd = open("/sys/devices/platform/virmouse/vmevent", O_RDWR);
	printf("%d\n", fd);
    lseek(fd, 0, SEEK_SET);
    printf("%d\n", (int) write(fd, cmd, 10));

	close(fd);
	return 0;
}
