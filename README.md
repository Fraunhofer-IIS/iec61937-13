# iec61937-13

The iec61937-13 project is a C based implementation of the [IEC 61937-13:2018 standard](https://webstore.iec.ch/publication/29325) and covers the encapsulation of MPEG-H bitstream in IEC data bursts.

For documentation and usage, please have a look at the [Wiki](https://github.com/Fraunhofer-IIS/iec61937-13/wiki/home).

## Supported architectures

**iec61937-13** is built and tested using the following architectures and toolchains:

<table>
  <tr>
    <th align="center" valign="center"><strong></strong></th>
    <th align="center" valign="center"><strong></strong>Architecture</th>
    <th align="center" valign="center"><strong></strong>Toolchain</th>
  </tr>
  <tr>
    <td><strong>Windows</strong></td>
    <td>
        x86<br />
        x86_64
    </td>
    <td>
        VS2019<br />
        VS2017
    </td>
  </tr>
  <tr>
    <td><strong>macOS</strong></td>
    <td>
        x86_64<br />
        arm64
    </td>
    <td>
        apple-clang 12.0.5 (Intel)<br />
        apple-clang 13.1.6 (ARM)
    </td>
  </tr>
  <tr>
    <td><strong>Linux</br>(Ubuntu 22.04)</strong></td>
    <td>
        x86<br />
        x86_64<br />
        RISC-V
    </td>
    <td>
        clang 14.0.0
    </td>
  </tr>
  <tr>
    <td><strong>Linux</br>(Debian 11)</strong></td>
    <td>
        x86<br />
        x86_64<br />
        arm32v7<br />
        arm64
    </td>
    <td>
        clang 11.0.1<br />
        gcc 10.2.1
    </td>
  </tr>
  <tr>
    <td><strong>Android</strong></td>
    <td>
        x86<br />
        x86_64<br />
        arm32v7<br />
        arm64
    </td>
    <td>
        clang 14.0.0
    </td>
  </tr>
  <tr>
    <td><strong>iOS</strong></td>
    <td>
        x86_64<br />
        arm64
    </td>
    <td>
        apple-clang 12.0.5
    </td>
  </tr>
</table>

Support for further platforms and architectures can be requested directly from [Fraunhofer IIS](https://www.iis.fraunhofer.de/en/ff/amm/broadcast-streaming/mpegh.html).

## Build

**iec61937-13** uses [CMake](https://cmake.org/) (version >= 3.16) to configure and build the project. A working CMake installation is therefore required to build the software.

The following project-specific CMake parameters are available:

<table>
<tr>
<td><code>iec61937-13_BUILD_BINARIES</code></td>
<td>Enable / Disable demo tool compilation.</td>
</tr>
<tr>
<td><code>iec61937-13_BUILD_DOC</code></td>
<td>

Enable / Disable documentation generation (requires a working [Doxygen](https://www.doxygen.nl/) installation).

</td>
</tr>
</table>

### How to build using CMake

Find below the basic instructions to build the project or visit the ["Wiki Build"](https://github.com/Fraunhofer-IIS/iec61937-13/wiki/Build-instructions) page(s) to get further information (additional build parameters and platform specific build instructions).

1. Clone the project and make a build folder.
   ```
   $ git clone https://github.com/Fraunhofer-IIS/iec61937-13.git
   $ mkdir -p build
   ```
2. Configure the project using CMake.
   ```
   $ cmake -S iec61937-13 -B build -DCMAKE_BUILD_TYPE=Release
   ```
3. Build the project.
   ```
   $ cmake --build build --config Release
   ```

# Command-line example programs

The **iec61937-13** provides following command-line-based examples programs:

- [IEC61937-13 encoder](https://github.com/Fraunhofer-IIS/iec61937-13/wiki/IEC61937-13-encoder-example)
- [IEC61937-13 decoder](https://github.com/Fraunhofer-IIS/iec61937-13/wiki/IEC61937-13-decoder-example)

## Contributing

Contributions may be done through a pull request to the upstream repository.

- Create a fork based on the latest master branch.
- Apply changes to the fork.
- Add the author names to [AUTHORS.md](./AUTHORS.md).
- Create a pull request to the upstream repository. The request must contain a detailed description of its purpose.

## Links

- [www.mpegh.com](https://mpegh.com)
- [Fraunhofer IIS MPEG-H Audio](https://www.iis.fraunhofer.de/en/ff/amm/broadcast-streaming/mpegh.html)

## License

Please see the [LICENSE.txt](./LICENSE.txt) file for the terms of use that apply to the software in this repository.

Fraunhofer supports the development of MPEG-H products and services by offering additional software, documentation, and technical advice. In addition, it operates the MPEG-H Trademark Program to ease interoperability testing of end-products. Please visit [www.mpegh.com](https://mpegh.com) for more information.

For more information, please contact amm-info@iis.fraunhofer.de
