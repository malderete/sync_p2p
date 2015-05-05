#ifndef CRC_H_
#define CRC_H_

#define MD5_BIN_PATH "/usr/bin/md5sum"


int crc_md5sum_wrapper(char *filename, char **md5sum);

#endif /* CRC_H_ */
