/*
// glgen.cpp - v0.4 - Generates OpenGL header file - Public Domain
// Metric Panda 2016 - http://metricpanda.com
//
// Command line utility that generates an OpenGL header file that contains
// typedefs and #defines for only the functions you actually use in your code
// using OpenGL API and Extension headers from
// https://www.opengl.org/registry/.
//
// It can optionally output boiler plate code that loads OpenGL based on
// https://github.com/skaslev/gl3w
//
// Example:
//    glgen source1.h source1.cpp source2.cpp -gl glcorearb.h \
//    -o opengl.generated.h \
//    -i glfwGetFramebufferSize,glfwMakeContextCurrent,glfwSwapInterval
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> // assert
#include <ctype.h> // toupper

#if _MSC_VER
  #define _CRT_SECURE_NO_WARNINGS 1
  #define WIN32_LEAN_AND_MEAN 1
  #define VC_EXTRALEAN 1
  #include <windows.h> // GetFileAttributesEx
#else
  #include <sys/stat.h> // stat
#endif

struct GLSettings
{
  char* HeadersStart;
  char* HeadersEnd;
  char* Output;
  char* Prefix;
  char** Inputs;
  char** Ignores;
  unsigned long long WriteTimestamp;
  int InputCount;
  int IgnoreCount;
  int Boilerplate;
  int Silent;
  int ForceGenerate;
};

static
int ParseCommandLine(GLSettings* Settings, int argc, char** argv);

static
int GenerateOpenGLHeader(GLSettings* Settings);

static
unsigned long long GetLastWriteTime(const char* Filename);

static
void FreeMemory(GLSettings* Settings);

static
void PrintHelp(char** argv)
{
  printf("Usage: %s [-h] -gl <registryfile> -o <outputfile> <inputfiles...>\n", argv[0]);
  printf("\nrequired arguments:\n");
  printf("  %-20s OpenGL header files (comma separated) downloaded from https://www.opengl.org/registry/\n", "-gl <filename1>,<filename2>");
  printf("  %-20s One or more input C/C++ files\n", "<inputfiles...>");
  printf("  %-20s Generated file containing typedefs and boilerplate code\n", "-o <filename>");
  printf("\noptional arguments:\n");
  printf("  %-20s Prints this help and exits\n", "-h");
  printf("  %-20s Suppress non error output.\n", "-silent");
  printf("  %-20s Force generation of header (ignores updated timestamp).\n", "-force");
  printf("  %-20s Function prefix for boilerplate code.\n", "-p <prefix>");
  printf("  %-20s Ignored tokens (comma separated).\n", "-i <token1,token2>");
  printf("  %-20s Don't generate OpenGL loading boilerplate code\n", "-no-b");
}

int main(int argc, char** argv)
{
  int Result = 0;
  if (argc < 4)
  {
    PrintHelp(argv);
    Result = 1;
  }
  else
  {
    GLSettings Settings = {};
    if (ParseCommandLine(&Settings, argc, argv))
    {
      Settings.WriteTimestamp = GetLastWriteTime(Settings.Output);
      unsigned long long MaxTimestamp = 0;
      for (int Index = 0; Index < Settings.InputCount; ++Index)
      {
        const char* InputFilename = Settings.Inputs[Index];
        unsigned long long LastWriteTime = GetLastWriteTime(InputFilename);
        if (LastWriteTime > MaxTimestamp)
        {
          MaxTimestamp = LastWriteTime;
        }
      }
      if (Settings.ForceGenerate || MaxTimestamp > Settings.WriteTimestamp)
      {
        Result = GenerateOpenGLHeader(&Settings);
      }
    }
    else
    {
      PrintHelp(argv);
      Result = 1;
    }
    FreeMemory(&Settings);
  }

  return Result;
}

static
unsigned long long GetLastWriteTime(const char* Filename)
{
  unsigned long long Result = 0;
#if _MSC_VER
  FILETIME LastWriteTime = {};
  WIN32_FILE_ATTRIBUTE_DATA Data;
  if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
  {
    LastWriteTime = Data.ftLastWriteTime;
  }
  Result = ((unsigned long long)LastWriteTime.dwLowDateTime) | ((unsigned long long)LastWriteTime.dwHighDateTime << 32);
#elif __APPLE__
  struct stat FileStat;
  if (stat(Filename, &FileStat) == 0)
  {
    Result = (unsigned long long)FileStat.st_mtimespec.tv_sec;
  }
#else
  struct stat FileStat;
  if (stat(Filename, &FileStat) == 0)
  {
    Result = (unsigned long long)FileStat.st_mtime;
  }
#endif
  return Result;
}

static
void FreeMemory(GLSettings* Settings)
{
  for (int Index = 0; Index < Settings->IgnoreCount; ++Index)
  {
    free(Settings->Ignores[Index]);
  }
  if (Settings->Ignores)
  {
    free(Settings->Ignores);
  }
  for (int Index = 0; Index < Settings->InputCount; ++Index)
  {
    free(Settings->Inputs[Index]);
  }
  if (Settings->Inputs)
  {
    free(Settings->Inputs);
  }
}

int ParseCommandLine(GLSettings* Settings, int argc, char** argv)
{
  assert(argc > 2);
  int Success = 0;
  int ShowHelp = 0;

  size_t InputMaxSize = sizeof(int)*(size_t)(argc-1);
  int* InputPositions = (int*)malloc(InputMaxSize);
  memset(InputPositions, -1, InputMaxSize);

  Settings->Boilerplate = 1;
  char* Ignores = 0;
  for (int Index = 1; Index < argc; ++Index)
  {
    char* Arg = argv[Index];
    if (Arg[0] == '-')
    {
      char* Option = Arg+1;
      if (strcmp(Option, "h") == 0)
      {
        ShowHelp = 1;
        break;
      }
      else if (strcmp(Option, "o") == 0 && Index < argc-1)
      {
        Settings->Output = argv[++Index];
      }
      else if (strcmp(Option, "gl") == 0 && Index < argc-1)
      {
        Settings->HeadersStart = argv[++Index];
        char* At = Settings->HeadersStart;
        while(*At)
        {
          if (*At == ',')
          {
            *At = 0;
          }
          At++;
        }
        Settings->HeadersEnd = At;
      }
      else if (strcmp(Option, "p") == 0 && Index < argc-1)
      {
        Settings->Prefix = argv[++Index];
      }
      else if (strcmp(Option, "force") == 0)
      {
        Settings->ForceGenerate = 1;
      }
      else if (strcmp(Option, "no-b") == 0)
      {
        Settings->Boilerplate = 0;
      }
      else if (strcmp(Option, "silent") == 0)
      {
        Settings->Silent = 1;
      }
      else if (strcmp(Option, "i") == 0)
      {
        Ignores = argv[++Index];
        char* At = Ignores;
        if (*At != ',')
        {
          Settings->IgnoreCount = 1;
        }
        while(*At)
        {
          if (*At == ',')
          {
            Settings->IgnoreCount++;
          }
          while(*At == ',')
          {
            At++;
          }
          At++;
        }
      }
    }
    else
    {
      InputPositions[Settings->InputCount++] = Index;
    }
  }
  if (!ShowHelp)
  {
    if (Settings->InputCount > 0)
    {
      Settings->Inputs = (char**)malloc(sizeof(char**) * (size_t)Settings->InputCount);
      for (int Index = 0; Index < Settings->InputCount; ++Index)
      {
        assert(InputPositions[Index] >= 0);
        char* Value = argv[InputPositions[Index]];
        size_t StringSize = (strlen(Value)+1)*sizeof(char*);
        Settings->Inputs[Index] = (char*)malloc(StringSize);
        memcpy(Settings->Inputs[Index], Value, StringSize);
      }
    }
    if (Settings->IgnoreCount > 0)
    {
      Settings->Ignores = (char**)malloc(sizeof(char**) * (size_t)Settings->IgnoreCount);
      char* Start = Ignores++;
      int ActualCount = 0;
      while(*Ignores)
      {
        if (*Ignores == ',' || *(Ignores+1) == 0)
        {
          int Length = (int)(Ignores-Start);
          if (*(Ignores+1) == 0)
          {
            Length++;
          }
          if (Length > 0)
          {
            assert(ActualCount < Settings->IgnoreCount);
            size_t StringSize = (size_t)(Length+1)*sizeof(char*);
            Settings->Ignores[ActualCount] = (char*)malloc(StringSize);
            memcpy(Settings->Ignores[ActualCount], Start, (size_t)(Length)*sizeof(char*));
            Settings->Ignores[ActualCount][Length] = 0;
            Start = ++Ignores;
            ActualCount++;
          }
        }
        while(*Ignores == ',')
        {
          Ignores++;
        }
        if (*Ignores)
        {
          Ignores++;
        }
      }
      Settings->IgnoreCount = ActualCount;
    }
    if (Settings->HeadersStart && Settings->Output && Settings->InputCount > 0)
    {
      Success = 1;
    }
  }
  free(InputPositions);

#if 0
  printf("======================\n");
  printf("Header:%s\n", Settings->Header);
  printf("Output:%s\n", Settings->Output);
  printf("Prefix:%s\n", Settings->Prefix);
  printf("Boilerplate:%i\n", Settings->Boilerplate);
  printf("%i Inputs:\n", Settings->InputCount);
  for (int Index = 0; Index < Settings->InputCount; ++Index)
  {
    printf("%s\n", Settings->Inputs[Index]);
  }
  printf("%i Ignores:\n", Settings->IgnoreCount);
  for (int Index = 0; Index < Settings->IgnoreCount; ++Index)
  {
    printf("%s\n", Settings->Ignores[Index]);
  }
  printf("======================\n");
#endif

  return Success;
}

#define ArraySize(array)    (sizeof(array)/sizeof(array[0]))
#if _WIN32
#define TermColorYellow    ""
#define TermColorReset     ""
#define TermColorGreen     ""
#else
#define TermColorYellow    "\e[33m"
#define TermColorReset     "\e[0m"
#define TermColorGreen     "\e[32m"
#endif
#define YELLOW(Text) TermColorYellow Text TermColorReset
#define GREEN(Text) TermColorGreen Text TermColorReset
#define PRI_STR ".*s"


struct GLString
{
  char* Chars;
  unsigned int Length;
};

struct GLToken
{
  GLString Value;
  unsigned int Hash;
};

struct GLArbToken
{
  GLString Value;
  GLString Line;
  GLString ReturnType;
  GLString FunctionName;
  GLString Parameters;
  unsigned int Hash;
};

struct GLTokenizer
{
  char* At;
};

static inline
int IsWhitespace(char c)
{
  int Result = (c == ' ' || c == '\t' || c == '\v' ||
                c == '\f');
  return Result;
}

static inline
int IsNewline(char c)
{
  int Result = (c == '\r') || (c == '\n');
  return Result;
}

static inline
int IsWhitespaceOrNewline(char c)
{
  int Result = IsNewline(c) || IsWhitespace(c);
  return Result;
}

static inline
int IsIdentifier(char c)
{
  int Result = ((c >= 'a') && (c <= 'z')) ||
               ((c >= 'A') && (c <= 'Z')) ||
               ((c >= '0') && (c <= '9')) ||
               (c == '_') || (c == '#') || (c == '*');
  return Result;
}

static inline
unsigned int GetStringHash(GLString& String)
{
  unsigned int Result = 1;
  unsigned char* RawStr = (unsigned char*)String.Chars;
  for(unsigned int Idx = 0; Idx < String.Length; ++Idx)
  {
    Result *= 0x01000193U;
    Result ^= (unsigned int)*RawStr++;
  }
  return Result;
}

static inline
void UpperCase(char* Output, GLString& Str)
{
  for (unsigned int Index = 0; Index < Str.Length; ++Index)
  {
    *Output++ = (char)toupper(Str.Chars[Index]);
  }
  *Output = 0;
}

static inline
GLArbToken ParseArbToken(GLTokenizer* Tokenizer)
{
  GLArbToken Token = {};
  while(*Tokenizer->At && (IsWhitespaceOrNewline(*Tokenizer->At) || !IsIdentifier(*Tokenizer->At)))
  {
    Tokenizer->At++;
  }
  Token.Value.Chars = Tokenizer->At;
  while(*Tokenizer->At && (IsIdentifier(*Tokenizer->At)))
  {
    Tokenizer->At++;
  }
  Token.Value.Length = (unsigned int)(Tokenizer->At - Token.Value.Chars);
  if (IsWhitespace(*Tokenizer->At) && *(Tokenizer->At + 1) == '*')
  {
    Tokenizer->At += 2;
    Token.Value.Length += 2;
  }
  return Token;
}

static inline
GLToken ParseToken(GLTokenizer* Tokenizer)
{
  GLToken Token = {};
  while(*Tokenizer->At && (IsWhitespaceOrNewline(*Tokenizer->At) || !IsIdentifier(*Tokenizer->At)))
  {
    Tokenizer->At++;
  }
  Token.Value.Chars = Tokenizer->At;
  while(*Tokenizer->At && IsIdentifier(*Tokenizer->At))
  {
    Tokenizer->At++;
  }
  Token.Value.Length = (unsigned int)(Tokenizer->At - Token.Value.Chars);
  Token.Hash = GetStringHash(Token.Value);
  return Token;
}

static inline
int IsUpperCase(char C)
{
  int Result = (C >= 'A') && (C <= 'Z');
  return Result;
}

static inline
int StartsWith(GLString Token, const char* Prefix)
{
  int Result = Token.Length > 0;
  for (unsigned int Index = 0; Index < Token.Length && *Prefix; ++Index)
  {
    if (*Prefix++ != Token.Chars[Index])
    {
      Result = 0;
      break;
    }
  }
  return Result;
}

static inline
int Equal(GLString Token, const char* Value)
{
  int Result = Token.Length > 0;
  if (Token.Length)
  {
    char* Chars = Token.Chars;
    unsigned int Index = 0;
    for (; Index < Token.Length && *Value; ++Index)
    {
      if (*Value++ != *Chars++)
      {
        Result = 0;
        break;
      }
    }
    Result = Index == Token.Length;
  }
  return Result;
}

#define TOKEN_HASH_SIZE 8192

static inline
GLArbToken* AddToken(GLArbToken* TokenHash, GLArbToken& Token)
{
  unsigned int Index = Token.Hash & (TOKEN_HASH_SIZE - 1);
  assert(Index < TOKEN_HASH_SIZE);
  GLArbToken* Matching = TokenHash + Index;

  int Iterations = 0;
  while(Matching->Hash && Iterations++ < TOKEN_HASH_SIZE)
  {
    Index = (Index + 1) & (TOKEN_HASH_SIZE - 1);
    Matching = TokenHash + Index;
  }
  if (Iterations < TOKEN_HASH_SIZE)
  {
    *Matching = Token;
  }
  else
  {
    Matching = 0;
  }
  return Matching;
}

static inline
void AdvanceToEndOfLine(GLTokenizer* Tokenizer)
{
  while(*Tokenizer->At && !IsNewline(*Tokenizer->At))
  {
    Tokenizer->At++;
  }
}

static inline
int AddToken(GLToken* TokenHash, GLToken& Token)
{
  unsigned int Index = Token.Hash & (TOKEN_HASH_SIZE - 1);
  GLToken* Matching = TokenHash + Index;
  assert(Index < TOKEN_HASH_SIZE);

  int Result = 0;
  int Iterations = 0;
  while(Matching->Hash && Iterations++ < TOKEN_HASH_SIZE)
  {
    Index = (Index + 1) & (TOKEN_HASH_SIZE - 1);
    Matching = TokenHash + Index;
  }
  if (Iterations < TOKEN_HASH_SIZE)
  {
    *Matching = Token;
  }
  return Result;
}

static inline
void AddCustomToken(GLToken* TokenHash, const char* Value)
{
  GLToken Token;
  GLString String;
  String.Chars = (char*)Value;
  String.Length = (unsigned int)strlen(Value);
  Token.Hash = GetStringHash(String);
  AddToken(TokenHash, Token);
}

static inline
GLArbToken* GetToken(GLArbToken* TokenHash, unsigned int Hash)
{
  GLArbToken* Matching = 0;
  if (Hash)
  {
    unsigned int Index = Hash & (TOKEN_HASH_SIZE - 1);
    Matching = TokenHash + Index;

    int Iterations = 0;
    while(Matching->Hash && Iterations++ < TOKEN_HASH_SIZE)
    {
      if (Matching->Hash == Hash)
      {
        break;
      }
      Index = (Index + 1) & (TOKEN_HASH_SIZE - 1);
      Matching = TokenHash + Index;
    }
    if (Matching->Hash != Hash)
    {
      Matching = 0;
    }
  }
  return Matching;
}

static inline
int Contains(GLToken* TokenHash, GLToken& Token)
{
  unsigned int Index = Token.Hash & (TOKEN_HASH_SIZE - 1);
  GLToken* Matching = TokenHash + Index;

  int Result = 0;
  int Iterations = 0;
  while(Matching->Hash && Iterations++ < TOKEN_HASH_SIZE)
  {
    if (Matching->Hash == Token.Hash)
    {
      Result = 1;
      break;
    }
    Index = (Index + 1) & (TOKEN_HASH_SIZE - 1);
    Matching = TokenHash + Index;
  }
  return Result;
}

int TokenComparer(const void* A, const void* B)
{
  GLToken* T1 = (GLToken*)A;
  GLToken* T2 = (GLToken*)B;
  int Result = 0;
  if (T1->Hash > T2->Hash)
  {
    Result = -1;
  }
  else if (T2->Hash > T1->Hash)
  {
    Result = 1;
  }
  return Result;
}

char* ReadEntireFile(char* Filename)
{
  char* Result = 0;
  FILE* File = fopen(Filename, "r");
  if (File)
  {
    fseek(File, 0, SEEK_END);
    long long Size = ftell(File);
    if (Size > 0)
    {
      fseek(File, 0, SEEK_SET);
      Result = (char*)malloc((size_t)(Size + 1));
      fread(Result, (size_t)Size, 1, File);
      Result[Size] = 0;
    }
    else
    {
      fprintf(stderr, "File is empty: %s", Filename);
    }
    fclose(File);
  }
  else
  {
    fprintf(stderr, "Couldn't open file: %s", Filename);
  }
  return Result;
}

char* ReadMultiFiles(char* Start, char* End)
{
  char* Result = 0;
  size_t RunningSize = 0;
  while(Start < End)
  {
    char* Filename = Start;
    FILE* File = fopen(Filename, "r");
    if (File)
    {
      fseek(File, 0, SEEK_END);
      long long Size = ftell(File);
      if (Size > 0)
      {
        fseek(File, 0, SEEK_SET);
        Result = (char*)realloc(Result, (size_t)Size + RunningSize + 1);
        fread(Result + RunningSize, (size_t)Size, 1, File);
        Result[RunningSize + (size_t)Size] = 0;
        if (RunningSize)
        {
          Result[RunningSize-2] = '\n';
          Result[RunningSize-1] = '\n';
        }
        RunningSize += (size_t)Size + 1;
      }
      else
      {
        fprintf(stderr, "File is empty: %s", Filename);
      }
      fclose(File);
    }
    else
    {
      fprintf(stderr, "Couldn't open file: %s", Filename);
    }
    Start += strlen(Start) + 1;
  }
  return Result;
}


static inline
int IsKnownOrIgnoredToken(GLArbToken* ArbHash, GLToken* Token, GLSettings* Settings)
{
  int Found = 0;
  GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
  if (ArbToken)
  {
    Found = 1;
  }
  else
  {
    for (int IgnoredIndex = 0; IgnoredIndex < Settings->IgnoreCount; ++IgnoredIndex)
    {
      if (Equal(Token->Value, Settings->Ignores[IgnoredIndex]))
      {
        Found = 1;
        break;
      }
    }
    if (!Found)
    {
      fprintf(stderr, YELLOW("WARNING") ": Token not found in header: %" PRI_STR "\n",
             Token->Value.Length, Token->Value.Chars);
    }
  }
  return Found;
}

static inline
int ParseFile(char* Filename, GLArbToken* ArbHash,
               GLToken* FunctionsHash, unsigned int* FunctionCount,
               GLToken* DefinesHash, unsigned int* DefinesCount,
               GLSettings* Settings)
{
  char* Data = ReadEntireFile(Filename);
  int Success = 0;
  if (Data)
  {
    GLTokenizer Tokenizer;
    Tokenizer.At = Data;
    while(*Tokenizer.At)
    {
      GLToken Token = ParseToken(&Tokenizer);
      if (StartsWith(Token.Value, "gl") && IsUpperCase(Token.Value.Chars[2]))
      {
        if (!Contains(FunctionsHash, Token) && IsKnownOrIgnoredToken(ArbHash, &Token, Settings))
        {
          AddToken(FunctionsHash, Token);
          *FunctionCount += 1;
        }
      }
      if (StartsWith(Token.Value, "GL_"))
      {
        if (!Contains(DefinesHash, Token) && IsKnownOrIgnoredToken(ArbHash, &Token, Settings))
        {
          AddToken(DefinesHash, Token);
          *DefinesCount += 1;
        }
      }
    }
    free(Data);
    Success = 1;
  }
  else
  {
    fprintf(stderr, "Couldn't open file %s", Filename);
  }
  return Success;
}


static
int GenerateOpenGLHeader(GLSettings* Settings)
{
  char* ArbData = ReadMultiFiles(Settings->HeadersStart, Settings->HeadersEnd);
  FILE* Output = fopen(Settings->Output, "w");
  const char* ProcPrefix = "GEN_";
  int Success = -1;

  if (Settings->InputCount <= 0)
  {
    fprintf(stderr, "Invalid input count");
  }
  else if (Output && ArbData)
  {
    GLArbToken* ArbHash = (GLArbToken*)calloc(sizeof(GLArbToken), TOKEN_HASH_SIZE);
    GLToken* FunctionsHash = (GLToken*)calloc(sizeof(GLToken), TOKEN_HASH_SIZE);
    GLToken* DefinesHash = (GLToken*)calloc(sizeof(GLToken), TOKEN_HASH_SIZE);
    unsigned int FunctionCount = 0;
    unsigned int DefinesCount = 0;
    unsigned int ArbTokenCount = 0;

    GLTokenizer Tokenizer;
    Tokenizer.At = ArbData;
    while(*Tokenizer.At)
    {
      GLArbToken Token = ParseArbToken(&Tokenizer);
      if (Equal(Token.Value, "GLAPI"))
      {
        char* Start = Token.Value.Chars;
        char* ReturnType = Tokenizer.At;
        Token = ParseArbToken(&Tokenizer);
        if (Equal(Token.Value, "const"))
        {
          ParseArbToken(&Tokenizer);
        }
        unsigned int ReturnTypeLength = (unsigned int)(Tokenizer.At - ReturnType);
        ParseArbToken(&Tokenizer);
        Token = ParseArbToken(&Tokenizer);
        Token.Hash = GetStringHash(Token.Value);
        if (!GetToken(ArbHash, Token.Hash))
        {
          ArbTokenCount++;
          GLArbToken* Result = AddToken(ArbHash, Token);
          Result->FunctionName.Chars = Token.Value.Chars;
          Result->FunctionName.Length = (unsigned int)(Tokenizer.At - Token.Value.Chars);

          Result->ReturnType.Chars = ReturnType;
          Result->ReturnType.Length = ReturnTypeLength;

          Result->Parameters.Chars = Tokenizer.At;
          AdvanceToEndOfLine(&Tokenizer);
          Result->Parameters.Length = (unsigned int)(Tokenizer.At - Result->Parameters.Chars);

          Result->Line.Chars = Start;
          Result->Line.Length = (unsigned int)(Tokenizer.At - Start);
        }
      }
      else if (StartsWith(Token.Value, "#define"))
      {
        char* Start = Token.Value.Chars;
        Token = ParseArbToken(&Tokenizer);
        Token.Hash = GetStringHash(Token.Value);
        if (!GetToken(ArbHash, Token.Hash))
        {
          ArbTokenCount++;
          GLArbToken* Result = AddToken(ArbHash, Token);
          AdvanceToEndOfLine(&Tokenizer);
          Result->Line.Chars = Start;
          Result->Line.Length = (unsigned int)(Tokenizer.At - Start);
        }
      }
    }

    {
      DefinesCount = 2;
      AddCustomToken(DefinesHash, "GL_MAJOR_VERSION");
      AddCustomToken(DefinesHash, "GL_MINOR_VERSION");
    }
    {
      FunctionCount = 1;
      AddCustomToken(FunctionsHash, "glGetIntegerv");
    }

    for (int Index = 0; Index < Settings->InputCount; ++Index)
    {
      ParseFile(Settings->Inputs[Index], ArbHash, FunctionsHash, &FunctionCount,
                DefinesHash, &DefinesCount, Settings);
    }

    qsort(FunctionsHash, TOKEN_HASH_SIZE, sizeof(GLToken), TokenComparer);
    qsort(DefinesHash, TOKEN_HASH_SIZE, sizeof(GLToken), TokenComparer);

    {
      const char* Prefix = "";
      if (Settings->Prefix)
      {
        Prefix = (const char*)Settings->Prefix;
      }
      //NOTE: Create defines
      const char* Generated =
        "#ifndef INCLUDE_OPENGL_GENERATED_H\n"
        "#define INCLUDE_OPENGL_GENERATED_H\n\n"
        "// NOTE: This file is generated automatically. Do not edit.\n"
        "// @GENERATED: %llu\n\n";
      fprintf(Output, Generated, Settings->WriteTimestamp);

      Generated =
        "typedef struct %sOpenGLVersion\n"
        "{\n"
        "  int Major;\n"
        "  int Minor;\n"
        "} %sOpenGLVersion;\n"
        "// Call this function to initialize OpenGL.\n"
        "// Example:\n"
        "//\n"
        "//    %sOpenGLVersion Version;\n"
        "//    %sOpenGLInit(&Version);\n"
        "//    if(Version.Major < 3)\n"
        "//    {\n"
        "//       printf(\"OpenGL 3 or above required.\\n\");\n"
        "//       return 0;\n"
        "//    }\n"
        "//\n"
        "static void %sOpenGLInit(%sOpenGLVersion* Version);\n\n\n";
      if (Settings->Boilerplate)
      {
        fprintf(Output, Generated, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix);
      }

      Generated =
        "#ifndef APIENTRY\n"
        "#define APIENTRY\n"
        "#endif\n"
        "#ifndef APIENTRYP\n"
        "#define APIENTRYP APIENTRY *\n"
        "#endif\n"
        "#ifndef GLAPI\n"
        "#define GLAPI extern\n"
        "#endif\n\n"

        "typedef void GLvoid;\n"
        "typedef unsigned int GLenum;\n"
        "typedef float GLfloat;\n"
        "typedef int GLint;\n"
        "typedef int GLsizei;\n"
        "typedef unsigned int GLbitfield;\n"
        "typedef double GLdouble;\n"
        "typedef unsigned int GLuint;\n"
        "typedef unsigned char GLboolean;\n"
        "typedef unsigned char GLubyte;\n"
        "typedef char GLchar;\n"
        "typedef short GLshort;\n"
        "typedef signed char GLbyte;\n"
        "typedef unsigned short GLushort;\n"
        "typedef ptrdiff_t GLsizeiptr;\n"
        "typedef ptrdiff_t GLintptr;\n"
        "typedef float GLclampf;\n"
        "typedef double GLclampd;\n"
        "typedef unsigned short GLhalf;\n\n";


      fprintf(Output, "%s", Generated);


      for (unsigned int Index = 0; Index < DefinesCount; ++Index)
      {
        GLToken* Token = DefinesHash + Index;
        assert(Token->Hash);
        GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
        if (ArbToken)
        {
          fwrite(ArbToken->Line.Chars, ArbToken->Line.Length + 1, 1, Output);
        }
      }
      const char* Spacer = "\n\n";
      fwrite(Spacer, strlen(Spacer), 1, Output);
      Generated =
        "typedef void (APIENTRY *GLDEBUGPROC)(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar *message,const void *userParam);\n";
      fprintf(Output, "%s", Generated);

      for (unsigned int Index = 0; Index < FunctionCount; ++Index)
      {
        GLToken* Token = FunctionsHash + Index;
        GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
        if (ArbToken)
        {
          char Name[512];
          UpperCase(Name, ArbToken->FunctionName);
          char Buffer[512];
          int Length = sprintf(Buffer, "typedef %" PRI_STR " (APIENTRYP PFN%sPROC) %" PRI_STR "\n",
                               ArbToken->ReturnType.Length, ArbToken->ReturnType.Chars,
                               Name,
                               ArbToken->Parameters.Length, ArbToken->Parameters.Chars);

          fwrite(Buffer, (size_t)Length, 1, Output);
        }
      }
      if (Settings->Boilerplate)
      {
        fwrite(Spacer, strlen(Spacer), 1, Output);
        for (unsigned int Index = 0; Index < FunctionCount; ++Index)
        {
          GLToken* Token = FunctionsHash + Index;
          GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
          if (ArbToken)
          {
            char Buffer[512];
            int Length = sprintf(Buffer, "#define %" PRI_STR " %s%" PRI_STR "\n",
                                 ArbToken->FunctionName.Length, ArbToken->FunctionName.Chars,
                                 ProcPrefix,
                                 ArbToken->FunctionName.Length, ArbToken->FunctionName.Chars);


            fwrite(Buffer, (size_t)Length, 1, Output);
          }
        }

        fwrite(Spacer, strlen(Spacer), 1, Output);
        for (unsigned int Index = 0; Index < FunctionCount; ++Index)
        {
          GLToken* Token = FunctionsHash + Index;
          GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
          if (ArbToken)
          {
            char Name[512];
            UpperCase(Name, ArbToken->FunctionName);
            char Buffer[512];
            int Length = sprintf(Buffer, "PFN%sPROC %s%" PRI_STR ";\n", Name, ProcPrefix,
                                 ArbToken->FunctionName.Length, ArbToken->FunctionName.Chars);

            fwrite(Buffer, (size_t)Length, 1, Output);
          }
        }

        Generated =
          "\n\n"
          "typedef void (*%sOpenGLProc)(void);\n\n"
          "#ifdef _WIN32\n"
          "static HMODULE %sOpenGLHandle;\n"
          "static void %sLoadOpenGL()\n"
          "{\n"
          "  %sOpenGLHandle = LoadLibraryA(\"opengl32.dll\");\n"
          "}\n"
          "static void %sUnloadOpenGL()\n"
          "{\n"
          "  FreeLibrary(%sOpenGLHandle);\n"
          "}\n"
          "static %sOpenGLProc %sOpenGLGetProc(const char *proc)\n"
          "{\n"
          "  %sOpenGLProc Result = (%sOpenGLProc)wglGetProcAddress(proc);\n"
          "  if (!Result)\n"
          "    Result = (%sOpenGLProc)GetProcAddress(%sOpenGLHandle, proc);\n"
          "  return Result;\n"
          "}\n"
          "#elif defined(__APPLE__) || defined(__APPLE_CC__)\n"
          "#include <Carbon/Carbon.h>\n"
          "\n"
          "static CFBundleRef GEN_Bundle;\n"
          "static CFURLRef GEN_BundleURL;\n"
          "\n"
          "static void %sLoadOpenGL()\n"
          "{\n"
          "  GEN_BundleURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,\n"
          "    CFSTR(\"/System/Library/Frameworks/OpenGL.framework\"),\n"
          "    kCFURLPOSIXPathStyle, 1);\n"
          "  GEN_Bundle = CFBundleCreate(kCFAllocatorDefault, GEN_BundleURL);\n"
          "}\n"
          "static void %sUnloadOpenGL()\n"
          "{\n"
          "  CFRelease(GEN_Bundle);\n"
          "  CFRelease(GEN_BundleURL);\n"
          "}\n"
          "static %sOpenGLProc %sOpenGLGetProc(const char *proc)\n"
          "{\n"
          "  CFStringRef ProcName = CFStringCreateWithCString(kCFAllocatorDefault, proc,\n"
          "    kCFStringEncodingASCII);\n"
          "  %sOpenGLProc Result = (%sOpenGLProc) CFBundleGetFunctionPointerForName(GEN_Bundle, ProcName);\n"
          "  CFRelease(ProcName);\n"
          "  return Result;\n"
          "}\n"
          "#else\n"
          "#include <dlfcn.h>\n"
          "\n"
          "static void *%sOpenGLHandle;\n"
          "typedef void (*__GLXextproc)(void);\n"
          "typedef __GLXextproc (* PFNGLXGETPROCADDRESSPROC) (const GLubyte *procName);\n"
          "static PFNGLXGETPROCADDRESSPROC glx_get_proc_address;\n"
          "static void %sLoadOpenGL()\n"
          "{\n"
          "  %sOpenGLHandle = dlopen(\"libGL.so.1\", RTLD_LAZY | RTLD_GLOBAL);\n"
          "  glx_get_proc_address = (PFNGLXGETPROCADDRESSPROC) dlsym(%sOpenGLHandle, \"glXGetProcAddressARB\");\n"
          "}\n"
          "static void %sUnloadOpenGL()\n"
          "{\n"
          "  dlclose(%sOpenGLHandle);\n"
          "}\n"
          "static %sOpenGLProc %sOpenGLGetProc(const char *proc)\n"
          "{\n"
          "  %sOpenGLProc Result = (%sOpenGLProc) glx_get_proc_address((const GLubyte *) proc);\n"
          "  if (!Result)\n"
          "    Result = (%sOpenGLProc) dlsym(%sOpenGLHandle, proc);\n"
          "  return Result;\n"
          "}\n"
          "#endif\n\n";
        fprintf(Output, Generated,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix,
                Prefix, Prefix, Prefix, Prefix, Prefix, Prefix, Prefix);
        Generated =
          "\n\nvoid %sOpenGLInit(%sOpenGLVersion* Version)\n"
          "{\n"
          "  %sLoadOpenGL();\n\n";
        fprintf(Output, Generated, Prefix, Prefix, Prefix);
        for (unsigned int Index = 0; Index < FunctionCount; ++Index)
        {
          GLToken* Token = FunctionsHash + Index;
          GLArbToken* ArbToken = GetToken(ArbHash, Token->Hash);
          if (ArbToken)
          {
            char Name[512];
            UpperCase(Name, ArbToken->FunctionName);
            char Buffer[512];
            int Length = sprintf(Buffer, "  %s%" PRI_STR " = (PFN%sPROC)%sOpenGLGetProc(\"%" PRI_STR "\");\n",
                                 ProcPrefix,
                                 ArbToken->FunctionName.Length, ArbToken->FunctionName.Chars,
                                 Name, Prefix, ArbToken->FunctionName.Length, ArbToken->FunctionName.Chars);

            fwrite(Buffer, (size_t)Length, 1, Output);
          }
        }
        Generated =
          "\n  %sUnloadOpenGL();\n"
          "\n"
          "  Version->Major = 0;\n"
          "  Version->Minor = 0;\n"
          "  if (glGetIntegerv)\n"
          "  {\n"
          "    glGetIntegerv(GL_MAJOR_VERSION, &Version->Major);\n"
          "    glGetIntegerv(GL_MINOR_VERSION, &Version->Minor);\n"
          "  }\n"
          "}\n\n"
          "#endif // INCLUDE_OPENGL_GENERATED_H\n";
        fprintf(Output, Generated, Prefix);
      }
      Success = 0;
      if (!Settings->Silent)
      {
        printf(GREEN("Completed!") " " GREEN("%u") " functions - " GREEN("%u") " defines - " GREEN("%u") " ARB tokens\n",
               FunctionCount, DefinesCount, ArbTokenCount);
      }
    }
  }

  if (Output)
  {
    fclose(Output);
  }

  if (ArbData)
  {
    free(ArbData);
  }

  return Success;
}

