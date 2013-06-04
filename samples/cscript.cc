#include <v8.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>

v8::Handle<v8::Context> CreateShellContext(v8::Isolate* isolate);
void RunShell(v8::Handle<v8::Context> context);
int RunMain(v8::Isolate* isolate, int argc, char* argv[]);
bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions);
v8::Handle<v8::Value> Echo(const v8::Arguments& args);

v8::Handle<v8::Value> GetArgumentFromNumber(const v8::Arguments& args);

v8::Handle<v8::Value> Write(const v8::Arguments& args);
v8::Handle<v8::Value> WriteLine(const v8::Arguments& args);
v8::Handle<v8::Value> WriteBlankLines(const v8::Arguments& args);
v8::Handle<v8::Value> StdOutClose(const v8::Arguments& args);

v8::Handle<v8::Value> getLineCount(v8::Local<v8::String> name, const v8::AccessorInfo& info);
v8::Handle<v8::Value> getColumnCount(v8::Local<v8::String> name, const v8::AccessorInfo& info);
v8::Handle<v8::Value> ReadLine(const v8::Arguments& args);
v8::Handle<v8::Value> Read(const v8::Arguments& args);
v8::Handle<v8::Value> ReadAll(const v8::Arguments& args);
v8::Handle<v8::Value> SkipLine(const v8::Arguments& args);
v8::Handle<v8::Value> Skip(const v8::Arguments& args);
v8::Handle<v8::Value> checkEndOfStream(v8::Local<v8::String> name, const v8::AccessorInfo& info);
v8::Handle<v8::Value> checkEndOfLine(v8::Local<v8::String> name, const v8::AccessorInfo& info);
v8::Handle<v8::Value> StdInClose(const v8::Arguments& args);

v8::Handle<v8::Value> newActiveXObject(const v8::Arguments& args);
v8::Handle<v8::Value> createTextFile(const v8::Arguments& args);

v8::Handle<v8::Value> Quit(const v8::Arguments& args);
v8::Handle<v8::String> ReadFile(const char* name);
void ReportException(v8::Isolate* isolate, v8::TryCatch* handler);

struct JSFile{
	FILE* fn;
	int column;
	int line;

	JSFile(){}

	JSFile(FILE* fn_, int column_, int line_){
		fn = fn_;
		column = column_;
		line = line_;
	}
};

static bool run_shell;
struct Arguments{
	int argLength;
	char** args;
} arguments;

struct StdIn{
	int columnCounter;
	int lineCounter;
	bool isClose;
} stdIn;

struct StdOut{
	bool isClose;
} stdOut;

int codeObjVarOrBlockDNSet = -2146828197;
int codeOutOfRange = -2146828279;
int codeIllegalType = -2146828275;
int codeIllegalArgs = -2146827838;
int codeObjDoesNSup = -2146827850;
int codeIsNFamily = -2146827837;

int main(int argc, char* argv[]) {
  v8::V8::SetFlagsFromCommandLine(&argc, argv, true);
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  run_shell = (argc == 1);
  arguments.argLength = argc - 1;
  arguments.args = new char*[arguments.argLength];
  stdIn.lineCounter = 1;
  stdIn.columnCounter = 1;
  stdIn.isClose = false;
  stdOut.isClose = false;
  int result=0;
  {
    v8::HandleScope handle_scope(isolate);
    v8::Handle<v8::Context> context = CreateShellContext(isolate);
    if (context.IsEmpty()) {
      fprintf(stderr, "Error creating context\n");
      return 1;
    }
    context->Enter();
    if (run_shell)
    	RunShell(context);
    else
    	result = RunMain(isolate, argc, argv);
    context->Exit();
  }
  v8::V8::Dispose();
  return result;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}


// Creates a new execution environment containing the built-in
// functions.
v8::Handle<v8::Context> CreateShellContext(v8::Isolate* isolate) {
  // Create a template for the global object.
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

  global->Set(v8::String::New("quit"), v8::FunctionTemplate::New(Quit));

  global->Set(v8::String::New("ActiveXObject"), v8::FunctionTemplate::New(newActiveXObject));

  v8::Handle<v8::ObjectTemplate> wshStdIn = v8::ObjectTemplate::New();
  wshStdIn->Set(v8::String::New("Read"), v8::FunctionTemplate::New(Read));
  wshStdIn->Set(v8::String::New("ReadAll"), v8::FunctionTemplate::New(ReadAll));
  wshStdIn->Set(v8::String::New("ReadLine"), v8::FunctionTemplate::New(ReadLine));
  wshStdIn->Set(v8::String::New("SkipLine"), v8::FunctionTemplate::New(SkipLine));
  wshStdIn->Set(v8::String::New("Skip"), v8::FunctionTemplate::New(Skip));
  wshStdIn->Set(v8::String::New("Close"), v8::FunctionTemplate::New(StdInClose));
  wshStdIn->SetAccessor(v8::String::New("AtEndOfStream"), checkEndOfStream);
  wshStdIn->SetAccessor(v8::String::New("AtEndOfLine"), checkEndOfLine);
  wshStdIn->SetAccessor(v8::String::New("Line"), getLineCount);
  wshStdIn->SetAccessor(v8::String::New("Column"), getColumnCount);

  v8::Handle<v8::ObjectTemplate> wshStdOut = v8::ObjectTemplate::New();
  wshStdOut->Set(v8::String::New("WriteLine"), v8::FunctionTemplate::New(WriteLine));
  wshStdOut->Set(v8::String::New("Write"), v8::FunctionTemplate::New(Write));
  wshStdOut->Set(v8::String::New("WriteBlankLines"), v8::FunctionTemplate::New(WriteBlankLines));
  wshStdOut->Set(v8::String::New("Close"), v8::FunctionTemplate::New(StdOutClose));

  v8::Handle<v8::FunctionTemplate> wshArguments = v8::FunctionTemplate::New(GetArgumentFromNumber);
  wshArguments->SetLength(arguments.argLength-1);
  if(arguments.argLength == 0)
	  wshArguments->SetLength(arguments.argLength);

  v8::Handle<v8::ObjectTemplate> wsh = v8::ObjectTemplate::New();
  wsh->Set(v8::String::New("Echo"), v8::FunctionTemplate::New(Echo));
  wsh->Set(v8::String::New("StdIn"), wshStdIn);
  wsh->Set(v8::String::New("StdOut"), wshStdOut);
  wsh->Set(v8::String::New("Arguments"), wshArguments);

  global->Set(v8::String::New("WSH"), wsh);
  global->Set(v8::String::New("WScript"), wsh);

  return v8::Context::New(isolate, NULL, global);
}

v8::Handle<v8::Value> getColumnCount(v8::Local<v8::String> name, const v8::AccessorInfo& info){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
	exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
	v8::Local<v8::Object> ex = exTemplate->NewInstance();
	if(stdIn.isClose == true)
		return v8::ThrowException(ex);
	return v8::Int32::New(stdIn.columnCounter);
}

v8::Handle<v8::Value> getLineCount(v8::Local<v8::String> name, const v8::AccessorInfo& info){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
	exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
	v8::Local<v8::Object> ex = exTemplate->NewInstance();
	if(stdIn.isClose == true)
		return v8::ThrowException(ex);
	return v8::Int32::New(stdIn.lineCounter);
}

v8::Handle<v8::Value> checkEndOfStream(v8::Local<v8::String> name, const v8::AccessorInfo& info){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
	exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
	v8::Local<v8::Object> ex = exTemplate->NewInstance();
	if(stdIn.isClose == true)
		return v8::ThrowException(ex);
	return v8::Boolean::New(std::cin.eof());
}

v8::Handle<v8::Value> checkEndOfLine(v8::Local<v8::String> name, const v8::AccessorInfo& info){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
	exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
	v8::Local<v8::Object> ex = exTemplate->NewInstance();
	if(stdIn.isClose == true)
		return v8::ThrowException(ex);
	char nextSymbol = std::cin.get();
	std::cin.unget();
	return v8::Boolean::New(nextSymbol == '\n');
}

v8::Handle<v8::Value> GetArguments(const v8::Arguments& args){//(v8::Local<v8::String> name, const v8::AccessorInfo& info){
	v8::Local<v8::Array> result = v8::Array::New(arguments.argLength);
	for(int i=0; i<arguments.argLength; i++)
		result->Set(i, v8::String::New(arguments.args[i]));
	return result;
}

v8::Handle<v8::Value> GetArgumentFromNumber(const v8::Arguments& args){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() == 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object does not support this property or method"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjDoesNSup));
	}
	if(args.Length() > 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
	}
	if(args.Length() > 1 || args.Length() ==0 ){
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	int index = args[0]->Int32Value();
	if(!args[0]->IsInt32()){
		if(index == 0){
			v8::String::Utf8Value argpointer(args[0]);
			char* arg = *argpointer;
			int i;
			for(i=0; i<argpointer.length();i++){
				if(arg[i] != '0')
					break;
			}
			if(i<argpointer.length()){
				exTemplate->Set(v8::String::New("description"), v8::String::New("Illegal type"));
				exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalType));
				v8::Local<v8::Object> ex = exTemplate->NewInstance();
				return v8::ThrowException(ex);
			}
		}
	}
	if(index >= arguments.argLength){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Index out of range"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeOutOfRange));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	char* result = arguments.args[index];
	return v8::String::New(result);
}

v8::Handle<v8::Value> Echo(const v8::Arguments& args) {
  bool first = true;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(args.GetIsolate());
    if (first) {
      first = false;
    } else {
      printf(" ");
    }
    v8::String::Utf8Value str(args[i]);
    const char* cstr = ToCString(str);
    if(args[i]->IsFalse()){
    	printf("%s", "0");
    	continue;
    }
    if(args[i]->IsTrue()){
    	printf("%s", "-1");
    	continue;
    }
    printf("%s", cstr);
  }
  printf("\n");
  fflush(stdout);
  return v8::Undefined();
}

v8::Handle<v8::Value> Skip(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(!args[0]->IsInt32()){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Illegal type"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalType));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdIn.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	Read(args);
	return v8::Undefined();
}

v8::Handle<v8::Value> StdInClose(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	stdIn.isClose = true;
	return v8::Undefined();
}

v8::Handle<v8::Value> StdOutClose(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	stdOut.isClose = true;
	return v8::Undefined();
}

v8::Handle<v8::Value> SkipLine(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdIn.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	std::string str;
	std::getline(std::cin, str);
	return v8::Undefined();
}

v8::Handle<v8::Value> WriteBlankLines(const v8::Arguments& args){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(!args[0]->IsInt32()){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Illegal type"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalType));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdOut.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	int counter = args[0]->Int32Value();
	for(int i=0; i<counter; i++)
		std::cout<<"\n";
	return v8::Undefined();
}

v8::Handle<v8::Value> WriteLine(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() > 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdOut.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	v8::HandleScope handle_scope(args.GetIsolate());
	v8::String::Utf8Value str(args[0]);
	const char* cstr = ToCString(str);
	if(args.Length() == 0)
		printf("\n");
	else
		printf("%s\n", cstr);
	fflush(stdout);
	return v8::Undefined();
}

v8::Handle<v8::Value> Write(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdOut.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	v8::HandleScope handle_scope(args.GetIsolate());
	v8::String::Utf8Value str(args[0]);
	const char* cstr = ToCString(str);
	printf("%s", cstr);
	fflush(stdout);
	return v8::Undefined();
}

v8::Handle<v8::Value> ReadLine(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdIn.isClose == true){
			exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
			exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
			v8::Local<v8::Object> ex = exTemplate->NewInstance();
			return v8::ThrowException(ex);
	}
	std::string str;
	std::getline(std::cin, str);
	int strLen = str.length();
	char* chars = new char[strLen + 1];
	chars[strLen] = '\0';
	for(int i=0; i<strLen; i++){
		chars[i]=str[i];
	}
	v8::Handle<v8::String> result = v8::String::New(chars, strLen);
	stdIn.columnCounter = 0;
	stdIn.lineCounter++;
	return result;
}

v8::Handle<v8::Value> Read(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 1){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(!args[0]->IsInt32()){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Illegal type"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalType));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdIn.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	int count = args[0]->Int32Value();
	char* chars = new char[count + 1];
	for(int i=0; i<count; i++)
		std::cin>>chars[i];
	chars[count] = '\0';
	v8::Handle<v8::String> result = v8::String::New(chars, count);
	stdIn.columnCounter++;
	return result;
}

v8::Handle<v8::Value> ReadAll(const v8::Arguments& args){
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 0){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	if(stdIn.isClose == true){
		exTemplate->Set(v8::String::New("description"), v8::String::New("Object variable or block With variable does'n set"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeObjVarOrBlockDNSet));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	int count = 0;
	std::vector<char> allChars;
	char currentChar;
	while(std::cin>>currentChar){
		count++;
		allChars.push_back(currentChar);
	}
	char* chars = new char[count + 1];
	for(int i=0; i<count; i++)
		chars[i] = allChars[i];
	chars[count] = '\0';
	v8::Handle<v8::String> result = v8::String::New(chars, count);
	stdIn.columnCounter++;
	return result;
}


v8::Handle<v8::Value> newActiveXObject(const v8::Arguments& args) {
	v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
	if(args.Length() != 1){
			exTemplate->Set(v8::String::New("description"), v8::String::New("Illegal type"));
			exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalType));
			v8::Local<v8::Object> ex = exTemplate->NewInstance();
			return v8::ThrowException(ex);
	}
	if(!args[0]->Equals(v8::String::New("Scripting.FileSystemObject"))){
		exTemplate->Set(v8::String::New("description"), v8::String::New("This Argument ins't supported"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(10000000));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	v8::Handle<v8::ObjectTemplate> axo = v8::ObjectTemplate::New();
	axo->Set(v8::String::New("CreateTextFile"), v8::FunctionTemplate::New(createTextFile));
	v8::Local<v8::Object> result = axo->NewInstance();
	return result;
}

v8::Handle<v8::Value> createTextFile(const v8::Arguments& args) {
	if(args.Length() == 0){
		v8::Handle<v8::ObjectTemplate> exTemplate = v8::ObjectTemplate::New();
		exTemplate->Set(v8::String::New("description"), v8::String::New("Invalid number of arguments or appropriation of property value"));
		exTemplate->Set(v8::String::New("number"), v8::Int32::New(codeIllegalArgs));
		v8::Local<v8::Object> ex = exTemplate->NewInstance();
		return v8::ThrowException(ex);
	}
	v8::String::Utf8Value file(args[0]);
	char* fileName = *file;
	FILE* newFile = fopen(fileName, "w");
	fclose(newFile);
	return v8::Undefined();
}

// Reads a file into a v8 string.
v8::Handle<v8::String> ReadFile(const char* name) {
  FILE* file = fopen(name, "rb");
  if (file == NULL)
	  return v8::Handle<v8::String>();
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
    i += read;
  }
  fclose(file);
  v8::Handle<v8::String> result = v8::String::New(chars, size);
  delete[] chars;
  return result;
}


// Process remaining command line arguments and execute files
int RunMain(v8::Isolate* isolate, int argc, char* argv[]) {
    const char* str = argv[1];
    for(int i=2; i<arguments.argLength+1; i++)
  	  arguments.args[i-2] = argv[i];
    arguments.argLength--;
    v8::Handle<v8::String> file_name = v8::String::New(str);
	v8::Handle<v8::String> source = ReadFile(str);
	if (source.IsEmpty()) {
	  fprintf(stderr, "Error reading '%s'\n", str);
	  return 1;
	}
	if (!ExecuteString(isolate, source, file_name, false, true)) return 1;
	return 0;
}

v8::Handle<v8::Value> Quit(const v8::Arguments& args) {
  // If not arguments are given args[0] will yield undefined which
  // converts to the integer value 0.
  int exit_code = args[0]->Int32Value();
  fflush(stdout);
  fflush(stderr);
  exit(exit_code);
  return v8::Undefined();
}


// The read-eval-execute loop of the shell.
void RunShell(v8::Handle<v8::Context> context) {
  fprintf(stderr, "V8 version %s extended[cscript]\n", v8::V8::GetVersion());
  static const int kBufferSize = 256;
  // Enter the execution environment before evaluating any code.
  v8::Context::Scope context_scope(context);
  v8::Local<v8::String> name(v8::String::New("(shell)"));
  while (true) {
    char buffer[kBufferSize];
    fprintf(stderr, "> ");
    char* str = fgets(buffer, kBufferSize, stdin);
    if (str == NULL) break;
    v8::HandleScope handle_scope(context->GetIsolate());
    ExecuteString(context->GetIsolate(),
                  v8::String::New(str),
                  name,
                  true,
                  true);
  }
  fprintf(stderr, "\n");
}


// Executes a string within the current v8 context.
bool ExecuteString(v8::Isolate* isolate,
                   v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions) {
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    // Echo errors that happened during compilation.
    if (report_exceptions)
      ReportException(isolate, &try_catch);
    return false;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      assert(try_catch.HasCaught());
      // Echo errors that happened during execution.
      if (report_exceptions)
        ReportException(isolate, &try_catch);
      return false;
    } else {
      assert(!try_catch.HasCaught());
      if (print_result && !result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        const char* cstr = ToCString(str);
        printf("%s\n", cstr);
      }
      return true;
    }
  }
}


void ReportException(v8::Isolate* isolate, v8::TryCatch* try_catch) {
  v8::HandleScope handle_scope(isolate);
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    fprintf(stderr, "%s\n", exception_string);
  } else {
    // Echo (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, exception_string);
    // Echo line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    fprintf(stderr, "%s\n", sourceline_string);
    // Echo wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      fprintf(stderr, " ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
    v8::String::Utf8Value stack_trace(try_catch->StackTrace());
    if (stack_trace.length() > 0) {
      const char* stack_trace_string = ToCString(stack_trace);
      fprintf(stderr, "%s\n", stack_trace_string);
    }
  }
}
