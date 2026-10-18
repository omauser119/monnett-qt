# Libraries in the Arch Linux binary archive

The application dynamically links the following unmodified Arch Linux libraries. They remain replaceable in `libexec/monnett-qt/lib`. Qt, libsecret, OpenSSL and other system runtime dependencies are not bundled.

| Library | Version used | License | Corresponding upstream source |
| --- | --- | --- | --- |
| libQuotient | 0.9.6.1 | LGPL-2.1-or-later | https://github.com/quotient-im/libQuotient/tree/0.9.6.1 |
| QtKeychain | 0.17.0 | BSD-3-Clause | https://github.com/frankosterfeld/qtkeychain/tree/0.17.0 |
| libolm | 3.2.16 | Apache-2.0 | https://gitlab.matrix.org/matrix-org/olm/-/tree/3.2.16 |

License texts are in `licenses/`. Arch package build recipes and downstream patches are available from https://gitlab.archlinux.org/archlinux/packaging/packages/libquotient, https://gitlab.archlinux.org/archlinux/packaging/packages/qtkeychain and https://gitlab.archlinux.org/archlinux/packaging/packages/libolm. The archive is intended for a compatible Arch x86_64 runtime with the same Qt ABI; source builds are preferred on other systems.

Monnett's name, service and published API contracts belong to their respective owners. This is an independent client. The application's own source is MIT-licensed; that license does not relicense dependencies or API contracts.
