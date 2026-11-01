The Weather provider is statically linked with these libraries; which
ones depend on the package:

x86_64-abiv11 (ABIv11 2026.09 SDK): libcurl 7.76.0 (curl.txt), OpenSSL
1.1.0h (openssl.txt), Zstandard 1.5.7 (zstd.txt) and zlib 1.3.1 (zlib.txt).

x86_64-v1 (mainline AROS SDK and contrib of 2026-10-26): libcurl 8.21.0
(curl-8.21.0.txt), OpenSSL 4.0.1 (openssl-4.0.1.txt, Apache License 2.0),
Brotli 1.1.0 decoder (brotli.txt), Zstandard 1.5.7 (zstd.txt) and zlib
1.3.1 (zlib.txt).

License texts were retrieved from the upstream releases listed below.

curl.txt: https://github.com/curl/curl/blob/curl-7_76_0/COPYING
openssl.txt: https://github.com/openssl/openssl/blob/OpenSSL_1_1_0h/LICENSE
zstd.txt: https://github.com/facebook/zstd/blob/v1.5.7/LICENSE
zlib.txt: https://github.com/madler/zlib/blob/v1.3.1/LICENSE
curl-8.21.0.txt: https://github.com/curl/curl/blob/curl-8_21_0/COPYING
openssl-4.0.1.txt: https://github.com/openssl/openssl/blob/openssl-4.0.1/LICENSE.txt
brotli.txt: https://github.com/google/brotli/blob/v1.1.0/LICENSE

This product includes software developed by the OpenSSL Project for use
in the OpenSSL Toolkit (https://www.openssl.org/).
This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com).
This product includes software written by Tim Hudson (tjh@cryptsoft.com).

Weather data by Open-Meteo, CC BY 4.0.
https://open-meteo.com/
https://creativecommons.org/licenses/by/4.0/
The free API endpoint has its own service terms; no commercial API
subscription or access key is included in this package.

All supplied condition SVG placeholders are original project artwork,
covered by the package MIT license. They may be replaced independently
of the provider. Future user-supplied artwork needs its own provenance.

ca-bundle.crt (certs/): Mozilla CA certificate store, MPL 2.0, see certs/README.txt.
