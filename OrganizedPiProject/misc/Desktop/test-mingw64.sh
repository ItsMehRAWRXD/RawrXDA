#!/bin/sh

PREFIX="./bin/"
VERSION="$(${PREFIX}gcc --version | awk '{ print $3; exit}')"
VERBOSE="no" # yes/no

CCEXE="mingw64-c.exe"
CC="gcc"
CFLAGS="-o ${CCEXE} -O2"

CXXEXE="mingw64-c++.exe"
CXX="g++"
CXXFLAGS="-o ${CXXEXE} -O2"

for mode in m32 m64 m32-static m64-static m32-e m64-e m32-e-static m64-e-static m32-ee m64-ee m32-ee-static m64-ee-static; do
echo -e "\e[1mInfo: Trying mode '${mode}'.\e[0m"

case "${mode}" in
	m32*)
		EFLAGS="-m32"
		cp -f "${PREFIX%/*}/../x86_64-w64-mingw32/lib/gcc/x86_64-w64-mingw32/${VERSION}/32/libstdc++-6-x86.dll" .
		;;
	m64*)
		EFLAGS="-m64"
		cp -f "${PREFIX%/*}/../x86_64-w64-mingw32/lib/gcc/x86_64-w64-mingw32/${VERSION}/libstdc++-6-x64.dll" .
		;;
esac

case "${mode}" in
	*-static)
		EFLAGS="${EFLAGS} -static"
		;;
esac

case "${mode}" in
	*-e*)
		EFLAGS="${EFLAGS} -flto -fuse-linker-plugin"
		;;
esac

case "${mode}" in
	*-ee*)
		EFLAGS="${EFLAGS} -fgraphite"
		;;
esac

[ "${VERBOSE}" = "yes" ] && echo ${PREFIX}${CC} ${CFLAGS} ${EFLAGS} -x c -
${PREFIX}${CC} ${CFLAGS} ${EFLAGS} -x c - <<'_EOF'
#include <stdlib.h>
#include <stdio.h>

__attribute__((used))
extern void __main ();

int main(int argc, char * argv[], char * envp[]) {
	fprintf(stdout, "Hello World!\n");
	return EXIT_SUCCESS;
}

_EOF
if [ $? -ne 0 ]; then
	echo "Error: Failed to build C application." >&2
	#exit 1
fi

./${CCEXE}
if [ $? -ne 0 ]; then
	echo "Error: Failed to run built C application." >&2
	#exit 1
fi

[ "${VERBOSE}" = "yes" ] && echo ${PREFIX}${CXX} ${CXXFLAGS} ${EFLAGS} -x c++ -
${PREFIX}${CXX} ${CXXFLAGS} ${EFLAGS} -x c++ - <<'_EOF'
#include <cstdlib>
#include <iostream>

__attribute__((used))
extern void __main ();

__attribute__((used))
int main(int argc, char * argv[], char * envp[]) {
	std::cout << "Hello World!" << std::endl;
	return EXIT_SUCCESS;
}

_EOF
if [ $? -ne 0 ]; then
	echo "Error: Failed to build C++ application." >&2
	#exit 1
fi

./${CXXEXE}
if [ $? -ne 0 ]; then
	echo "Error: Failed to run built C++ application." >&2
	#exit 1
fi
done
