// smart_plug/components/mqtt_manager/include/aws_certs.h
#ifndef AWS_CERTS_H
#define AWS_CERTS_H

#include <stddef.h>  

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AWS IoT Certificates
 * 
 * These are embedded as strings in aws_certs.c
 */

extern const char aws_cert_ca[];        // Amazon Root CA 1
extern const char aws_cert_crt[];        // Device certificate
extern const char aws_cert_private[];     // Device private key

extern const size_t aws_cert_ca_len;
extern const size_t aws_cert_crt_len;
extern const size_t aws_cert_private_len;

#ifdef __cplusplus
}
#endif

#endif /* AWS_CERTS_H */