#ifndef BINARY_H_
#define BINARY_H_

struct range;

/* 0=not found, 1=found(thumb), 2=found(arm), -1=found(thumb) continue, -2=found(arm) continue */
typedef int (*callback_t)(const unsigned char *p, uint32_t size, va_list ap, uint64_t addr, void *user);

uint64_t parse_symbols(const unsigned char *p, const char *key);
uint64_t parse_gadgets(const struct range *ranges, const unsigned char *p, void *user, callback_t callback, ...);
struct range *parse_ranges(const unsigned char *p);
void delete_ranges(struct range *ranges);

int is_LOAD_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_LOAD_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_LOAD_R0R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_LOAD_R0R3(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_LOAD_R4R5(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_LDR_R0_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_STR_R0_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_ADD_R0_R1(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_BLX_R4(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_BLX_R4_SP(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_RET_0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_ADD_SP(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_MOV_Rx_R0(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_MOV_R0_Rx(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_COMPARE(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_ldmia(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_ldmiaw(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);
int is_string(const unsigned char *buf, uint32_t sz, va_list ap, uint64_t addr, void *user);

#endif
