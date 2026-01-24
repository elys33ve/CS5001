#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <sys/random.h>

char write_buf[4096];
char read_buf[4096];

int main(int argc, char *argv[]) {
   ssize_t bytes_read;
   ssize_t bytes_written;
   int x;
   int fd;
   int frame_size_bytes;
   int test_cnt_start=1;
   char test_cnt;
   int ptr;
   int frame_cnt=0;
   int byte_cnt=0;
   ssize_t n;
   int seq=0;
   int errors=0;
   int iter=0;

   if (argc != 2) {
      fprintf(stderr, "Usage: %s <frame size  bytes>\n", argv[0]);
      return EXIT_FAILURE;
   }
   frame_size_bytes = atoi(argv[1]);
   if (frame_size_bytes < 0 || frame_size_bytes > 4095) {
      fprintf(stderr, "Invalid frame size. Must be between 0 and 4095.\n");
      return EXIT_FAILURE;
   }
   printf("frame_size_bytes = %d\n", frame_size_bytes);

   // open device in read mode
   fd = open("/dev/hwaccel0", O_RDWR);
   if (fd < 0) {
      perror("Failed to open device");
      return EXIT_FAILURE;
   }
   memset(write_buf, 0, 4096);
   memset(read_buf, 0, 4096);

   // main read loop
   while (1) {
      do {
         n = getrandom(write_buf, frame_size_bytes, 0);
         if (n) {
            bytes_written = write(fd, write_buf, frame_size_bytes);
            if (bytes_written > 0 && bytes_written < frame_size_bytes) {
               printf("written = %d\n", bytes_written);
            }
         }
         else {
            perror("getrandom");
         }

         do {
            bytes_read = read(fd, read_buf, frame_size_bytes);
            if (bytes_read < 0) {
               perror("Error reading from device");
               break;
            }
         } while (!bytes_read);
         frame_cnt++;
         byte_cnt++;
         if (bytes_read == bytes_written) {
            seq++;
            iter++;
            if (memcmp(write_buf, read_buf, 2048)) {
               errors++;
               for (x=0; x<20; x++) {
                  printf("%d: 0x%x, 0x%x\n", x, (unsigned)write_buf[x], (unsigned)read_buf[x]);
               }
            }
            if ((iter % 10000) == 0) {
               printf("errors = %u\n", (unsigned)errors);
            }
         }
         else {
            printf("read = %d, written = %d\n", bytes_read, bytes_written);
         }
      } while (bytes_read);
      usleep(100); // small delay
   }

   close(fd);
   return 0;
}
