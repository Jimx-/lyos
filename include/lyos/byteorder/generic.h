#ifndef _BYTEORDER_GENERIC_H_
#define _BYTEORDER_GENERIC_H_

#define be32_to_cpup __be32_to_cpup
#define cpu_to_be32p __cpu_to_be32p

#define be32_to_cpu __be32_to_cpu
#define cpu_to_be32 __cpu_to_be32

#define le16_to_cpu __le16_to_cpu
#define le32_to_cpu __le32_to_cpu
#define le64_to_cpu __le64_to_cpu
#define cpu_to_le16 __cpu_to_le16
#define cpu_to_le32 __cpu_to_le32
#define cpu_to_le64 __cpu_to_le64

#endif // _BYTEORDER_GENERIC_H_
