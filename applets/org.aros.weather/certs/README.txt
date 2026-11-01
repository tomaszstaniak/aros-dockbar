ca-bundle.crt: the Mozilla CA certificate store (192 certificates), as
packaged by Homebrew ca-certificates 2026-09-25 (https://curl.se/docs/caextract.html).
It lets Weather verify HTTPS servers on systems whose own bundle
(ENV:SYS/Certificates/ca-bundle.crt) is missing or old. The provider tries
this file first, then a file named by CURL_CA_BUNDLE, then the system's;
verification is never turned off.
Licence: Mozilla Public License 2.0 (https://mozilla.org/MPL/2.0/).
Replace it with a newer extract when Mozilla's store changes.
