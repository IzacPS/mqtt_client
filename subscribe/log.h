#ifndef SUBSCRIBE_LOG_H
#define SUBSCRIBE_LOG_H

#define LOG_ERROR(msg, ...) fprintf(stderr, "ERROR!\n    file: %s function: %s line: %d\n    descrição: " msg "\n", __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define LOG_WARNING(msg, ...) printf("WARNING!\n    file: %s function: %s line: %d\n    descrição: " msg "\n", __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define LOG_INFO(msg, ...) printf("INFO!\n    file: %s function: %s line: %d\n    descrição: " msg "\n", __FILE__, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define LOG(str, ...) printf("LOG: " str, ## __VA_ARGS__)


#endif //SUBSCRIBE_LOG_H