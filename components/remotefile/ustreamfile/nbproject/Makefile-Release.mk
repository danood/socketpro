#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Environment
MKDIR=mkdir
CP=cp
GREP=grep
NM=nm
CCADMIN=CCadmin
RANLIB=ranlib
CC=gcc
CCC=g++
CXX=g++
FC=gfortran
AS=as

# Macros
CND_PLATFORM=GNU-Linux-x86
CND_DLIB_EXT=so
CND_CONF=Release
CND_DISTDIR=dist
CND_BUILDDIR=build

# Include project Makefile
include Makefile

# Object Directory
OBJECTDIR=${CND_BUILDDIR}/${CND_CONF}/${CND_PLATFORM}

# Object Files
OBJECTFILES= \
	${OBJECTDIR}/_ext/562988696/aserverw.o \
	${OBJECTDIR}/_ext/1429980336/sfileimpl.o \
	${OBJECTDIR}/_ext/1429980336/ustreamfile.o \
	${OBJECTDIR}/_ext/562988696/membuffer.o


# C Compiler Flags
CFLAGS=-std=c++11 -static-libgcc -static-libstdc++

# CC Compiler Flags
CCFLAGS=-std=c++11 -static-libgcc -static-libstdc++
CXXFLAGS=-std=c++11 -static-libgcc -static-libstdc++

# Fortran Compiler Flags
FFLAGS=

# Assembler Flags
ASFLAGS=

# Link Libraries and Options
LDLIBSOPTIONS=-luservercore

# Build Targets
.build-conf: ${BUILD_SUBPROJECTS}
	"${MAKE}"  -f nbproject/Makefile-${CND_CONF}.mk ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/libustreamfile.${CND_DLIB_EXT}

${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/libustreamfile.${CND_DLIB_EXT}: ${OBJECTFILES}
	${MKDIR} -p ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}
	g++ -o ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/libustreamfile.${CND_DLIB_EXT} ${OBJECTFILES} ${LDLIBSOPTIONS} -std=c++11 -static-libgcc -static-libstdc++ -shared -s -fPIC

${OBJECTDIR}/_ext/562988696/aserverw.o: ../../../../include/aserverw.cpp 
	${MKDIR} -p ${OBJECTDIR}/_ext/562988696
	${RM} "$@.d"
	$(COMPILE.c) -O2 -s -DNDEBUG -fPIC  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/562988696/aserverw.o ../../../../include/aserverw.cpp

${OBJECTDIR}/_ext/1429980336/sfileimpl.o: ../../../../include/file/server_impl/sfileimpl.cpp 
	${MKDIR} -p ${OBJECTDIR}/_ext/1429980336
	${RM} "$@.d"
	$(COMPILE.c) -O2 -s -DNDEBUG -fPIC  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/1429980336/sfileimpl.o ../../../../include/file/server_impl/sfileimpl.cpp

${OBJECTDIR}/_ext/1429980336/ustreamfile.o: ../../../../include/file/server_impl/ustreamfile.cpp 
	${MKDIR} -p ${OBJECTDIR}/_ext/1429980336
	${RM} "$@.d"
	$(COMPILE.c) -O2 -s -DNDEBUG -fPIC  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/1429980336/ustreamfile.o ../../../../include/file/server_impl/ustreamfile.cpp

${OBJECTDIR}/_ext/562988696/membuffer.o: ../../../../include/membuffer.cpp 
	${MKDIR} -p ${OBJECTDIR}/_ext/562988696
	${RM} "$@.d"
	$(COMPILE.c) -O2 -s -DNDEBUG -fPIC  -MMD -MP -MF "$@.d" -o ${OBJECTDIR}/_ext/562988696/membuffer.o ../../../../include/membuffer.cpp

# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r ${CND_BUILDDIR}/${CND_CONF}
	${RM} ${CND_DISTDIR}/${CND_CONF}/${CND_PLATFORM}/libustreamfile.${CND_DLIB_EXT}

# Subprojects
.clean-subprojects:

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
