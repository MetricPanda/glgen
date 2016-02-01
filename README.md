# glgen 

*glgen* is a minimal OpenGL extension loader that parses your project files and generates a header file based on the functions you actually use.

The definitions are extracted from header files provided by the [Khronos OpenGL API registry](https://www.opengl.org/registry/).

The boilerplate code that initializes OpenGL and loads all function pointers is minimal and cross platform.

## Usage

glgen requires an API registry header file that you can download from the [Khronos OpenGL API registry](https://www.opengl.org/registry/).
You must also specify the output file with the `-o` option, and one or more `<inputfile>` to parse.

```
Usage: glgen [-h] -gl <registryfile> -o <outputfile> <inputfiles...>

required arguments:
  -gl <filename>       OpenGL header file downloaded from https://www.opengl.org/registry/
  <inputfiles...>      One or more input C/C++ files
  -o <filename>        Generated file containing typedefs and boilerplate code

optional arguments:
  -h                   Prints this help and exits
  -p <prefix>          Function prefix for boilerplate code.
  -i <token1,token2>   Ignored tokens (comma separated).
  -no-b                Don't generate the OpenGL loading boilerplate code
```

The generated boilerplate code that initializes OpenGL can be used like so:


``` cpp
OpenGLVersion Version;
OpenGLInit(&Version);
if(Version.Major < 3)
{
  printf("OpenGL 3 or above required.\n");
  return 0;
}
```

This is the definition of the `OpenGLVersion` struct:
``` cpp
struct OpenGLVersion
{
  int Major;
  int Minor;
};
```

To avoid collisions with your code you can specify a prefix with the `-p` option like so:

```
glgen source1.h source1.cpp source2.cpp -gl glcorearb.h \
  -o opengl.generated.h -p PREFIX_\
  -i glfwGetFramebufferSize,glfwMakeContextCurrent,glfwSwapInterval
```

And the following code will be generated:

``` cpp
struct PREFIX_OpenGLVersion
{
  int Major;
  int Minor;
};

static void PREFIX_OpenGLInit(PREFIX_OpenGLVersion* Version);
```

## Running as part of your build

You can run glgen just before your normal build to keep the generated header up to data. For example, you can add the following to your `CMakeLists.txt` and glgen will be integrated in your build:

``` CMake

set(PREPROCESS_OPENGL_SOURCES ${CMAKE_SOURCE_DIR}/src/file1.cpp ${CMAKE_SOURCE_DIR}/src/file1.h)
set(PREPROCESS_OPENGL_OUTPUT ${CMAKE_SOURCE_DIR}/src/opengl.generated.h)
add_custom_target(
  OpenGLPreprocess
  COMMENT "Preprocessing OpenGL"
  COMMAND glgen ${PREPROCESS_OPENGL_SOURCES} -gl ${CMAKE_SOURCE_DIR}/src/lib/glcorearb.h -o ${PREPROCESS_OPENGL_OUTPUT} -p PREFIX_ -i glfwGetFramebufferSize,glfwMakeContextCurrent,glfwSwapInterval
)
add_dependencies(${EXECUTABLE} OpenGLPreprocess)

```

## License

glgen is in the public domain. See the file [LICENSE](LICENSE) for more information.

## Copyright

[OpenGL](http://www.opengl.org/) is a registered trademark of [SGI](http://www.sgi.com/).

