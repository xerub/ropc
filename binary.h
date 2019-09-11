#ifndef BINARY_H_
#define BINARY_H_

struct range;

typedef int (*callback_t)(const unsigned char *p, uint32_t size, uint64_t addr, void *user);

uint64_t parse_symbols(const unsigned char *p, const char *key);
uint64_t parse_string(const struct range *ranges, const unsigned char *p, void *user, callback_t callback, const char *str);
struct range *parse_ranges(const unsigned char *p, size_t sz);
void delete_ranges(struct range *ranges);

#endif
