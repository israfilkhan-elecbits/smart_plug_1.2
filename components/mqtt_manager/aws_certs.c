// smart_plug/components/mqtt_manager/aws_certs.c
#include <stddef.h> 
#include "aws_certs.h"

/*===============================================================================
  Amazon Root CA 1 (RSA 2048) 
  ===============================================================================*/

const char aws_cert_ca[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"
"rqXRfboQnoZsG4q5WTP468SQvvG5\n"
"-----END CERTIFICATE-----\n";

const size_t aws_cert_ca_len = sizeof(aws_cert_ca) - 1;

/*===============================================================================
  Device Certificate 
  ===============================================================================*/

const char aws_cert_crt[] = 
"-----BEGIN CERTIFICATE-----\n"
"MIIDWTCCAkGgAwIBAgIUeS5bQl76F70NiBlttQD8xVnNpMMwDQYJKoZIhvcNAQEL\n"
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\n"
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTI2MDExNjEwNTIx\n"
"OFoXDTQ5MTIzMTIzNTk1OVowHjEcMBoGA1UEAwwTQVdTIElvVCBDZXJ0aWZpY2F0\n"
"ZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKaTiYcMHsf/GxC5qnnq\n"
"vvPCALBgX1VCfJFlgHNdDDhMqwyPfABSOGReQMH75sUnPHLYCBYmDJ+Gc4ccS3s+\n"
"7UT04MnC8Qy9/BwKJ1aQO/ioY52Wa7v7YE11NfKMtoAV475P+KZclUIMiic5CjYC\n"
"k+r985qxC5y4K5PjHXDQst5JbXjhio9baC3CGNtSEFjGohSB/xhHYi9VExu36ECm\n"
"CV9ni/7DWNB4f30aJhLCvumZsDwCqVezmEtlF3QzubV/LgYVd45eFRBsjCrAJ1RD\n"
"g4R1a7ORa1GTKLkpfeobUttNvwOQacIiddkwo0KP6/6ZWrG6iRAcOKIH7VEfj17c\n"
"sXMCAwEAAaNgMF4wHwYDVR0jBBgwFoAUppu4wP2bXeqxGFkjqcAjJXO4yS4wHQYD\n"
"VR0OBBYEFFlJx3RZNCwKx5GaqwDJtjMTxA6vMAwGA1UdEwEB/wQCMAAwDgYDVR0P\n"
"AQH/BAQDAgeAMA0GCSqGSIb3DQEBCwUAA4IBAQBtqgj8WzVSVdEjpnt/BfoSXwck\n"
"wlhJIzbexQlSpP/RBYVzciYq2jTseNiG06k3UEW+n/qUhG4fRd6lunseLPfjoYZn\n"
"1MTLh2HQw8ND2GW6TJICQ9idrNs2qZ+NCJL/9zN38AEYZf0TmOlzyZ7CTbx87fIY\n"
"TG+1VJmIrG2dQhRH3DMGDioIzZUFMm782d7EzP3/QKmapRaY1X5LTg4SrD3TJro4\n"
"6lhE2K6uru0fZ0x2e3vHI0Uga8H76MuSXbR5ss9NHx72Z+hE9a+PvFdG8l3aGc2A\n"
"baJwjyAuf4GgJjzSIEZ4O40qb3Vo8wIfDuO62abKws+txI3PNkEh7j1DkqVm\n"
"-----END CERTIFICATE-----\n";

const size_t aws_cert_crt_len = sizeof(aws_cert_crt) - 1;

/*===============================================================================
  Device Private Key 
  ===============================================================================*/

const char aws_cert_private[] = 
"-----BEGIN RSA PRIVATE KEY-----\n"
"MIIEowIBAAKCAQEAppOJhwwex/8bELmqeeq+88IAsGBfVUJ8kWWAc10MOEyrDI98\n"
"AFI4ZF5AwfvmxSc8ctgIFiYMn4ZzhxxLez7tRPTgycLxDL38HAonVpA7+KhjnZZr\n"
"u/tgTXU18oy2gBXjvk/4plyVQgyKJzkKNgKT6v3zmrELnLgrk+MdcNCy3klteOGK\n"
"j1toLcIY21IQWMaiFIH/GEdiL1UTG7foQKYJX2eL/sNY0Hh/fRomEsK+6ZmwPAKp\n"
"V7OYS2UXdDO5tX8uBhV3jl4VEGyMKsAnVEODhHVrs5FrUZMouSl96htS202/A5Bp\n"
"wiJ12TCjQo/r/plasbqJEBw4ogftUR+PXtyxcwIDAQABAoIBAGTzAqSiNsFTm+5t\n"
"5p+OIP0OtGYvcXb1HRLsZYUEfdRcukiZaDe1nFFPQYWOCJOwrJSY0YXCt2GyFK9r\n"
"+V6OizKACP6dMoJbXL8NdDukm4OdYQlu0ImS1RD8GJ6OokdLfMKoKnN/pkDp4ovU\n"
"qJiExWnjT0+PPg9TGa29NOlawRuuf8MkcvmS6RwFrawucN+7qjT6htiAUdt7Nk+F\n"
"ibnfM7Sws/VdmA8Rqq4Z411Y8i25FAOoeiRNh12WYI0TM+3G43vDrAd4o1MoCIXo\n"
"QMncZgfBUd8JkTMnH6+ifABMcK384UbUgNFwCukK/x/dQKdnWUVJvETr82t0HIRG\n"
"kwBWhmkCgYEA1ecPTHxJHMFYfHNzTTtBlfNNQ8x1bJw9xHwC8EsjzkydnJyEnif0\n"
"0I73PtBhRFwt2r9l9CKIj87lWgEgoU6tUz+jvbjiBeSPG+5PvfvI1ay7WLT1Mu8B\n"
"rc2iO/dvOWyYy1aSNaRm1YvPfViE3Cb/uor5eP5SQqR+yI5hrxxLRK8CgYEAx1wQ\n"
"RjIgipwznJZNtcCg8BmfCJxqNwmS25dFRWnGY6fVk/9g+nPbB4AwphlkJNCt726Y\n"
"orDeJvKO7MvGLGivBv6GxARdmUG+2YMzS9fmsFEaC5EioqSBNL+ReEQ0z4szAb3u\n"
"9kVeKaarx9kWvpJheT3a6z96+Wq1OarxptmAWH0CgYAahne4NWVfon5VmH+A4UtF\n"
"zBEVykH5gPqL5hD7OWYsTAXziNlNP4k1X7U7Xd3h+0hYawm6l5m1s6NvYNpqBnap\n"
"7ydf/JBSyMASZ6AN4C5MiQoGexI5Cbh8lBZ9NzbcuSHNfWPOMR/rdVX6pkJ7hn6J\n"
"5HgBUBBlYT6zoixs6aZP0wKBgAyRvWTvnCWhG4/v2g5virYYp3I/imLV87BspS1v\n"
"MdbuqgSewVqJG3IpnueaCjpX/d9utajsRdVmzaQYZPI/12k1ewG41L3o60ODhiRu\n"
"BFlxg5bfG7Ptc0gEHAPdKQc824ZslzhnvzwZChObmFeDmymtwLO8WOCI3cw4/utq\n"
"IzFxAoGBALlyzOzTpPfOczZz6mhyRqYyZrBGvcT2ckF4NWEP1E3l2CzjMDQ5tOc3\n"
"2Lxk0FuxdiU62KBwSkSMkDmxLQtGpn9uGvoCzw5O9aHhW9JtejwVNllbiS4B6+io\n"
"VVS5zI8MCFFugjvpS1XVE9PweeamcHCXxZeNltO9tpl5B8r5vb4K\n"
"-----END RSA PRIVATE KEY-----\n";

const size_t aws_cert_private_len = sizeof(aws_cert_private) - 1;