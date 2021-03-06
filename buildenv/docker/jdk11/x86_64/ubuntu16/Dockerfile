# Copyright (c) 2018, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception

# To use this docker file:
#   docker build -t=openj9 .
#   docker run -it openj9

FROM ubuntu:16.04

# Install required OS tools
RUN apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    software-properties-common \
    python-software-properties \
  && add-apt-repository ppa:ubuntu-toolchain-r/test \
  && apt-get update \
  && apt-get install -qq -y --no-install-recommends \
    gcc-7 \
    g++-7 \
    autoconf \
    ca-certificates \
    ccache \
    cmake \
    cpio \
    file \
    git \
    git-core \
    libasound2-dev \
    libcups2-dev \
    libdwarf-dev \
    libelf-dev \
    libfontconfig1-dev \
    libfreetype6-dev \
    libnuma-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxt-dev \
    libxtst-dev \
    make \
    pkg-config \
    realpath \
    ssh \
    unzip \
    wget \
    zip \
    vim \
  && rm -rf /var/lib/apt/lists/*

# Create links for c++,g++,cc,gcc
RUN ln -s g++ /usr/bin/c++ \
  && ln -s g++-7 /usr/bin/g++ \
  && ln -s gcc /usr/bin/cc \
  && ln -s gcc-7 /usr/bin/gcc

# Download and setup freemarker.jar to /root/freemarker.jar
RUN cd /root \
  && wget https://sourceforge.net/projects/freemarker/files/freemarker/2.3.8/freemarker-2.3.8.tar.gz/download -O freemarker.tgz \
  && tar -xzf freemarker.tgz freemarker-2.3.8/lib/freemarker.jar --strip=2 \
  && rm -f freemarker.tgz

# Download and install boot JDK from AdoptOpenJDK
# Currently, no JDK10 with OpenJ9 releases are available at AdoptOpenJDK. 
# So, the AdoptOpenJDK API can't be used to download a boot JDK. 
# A nightly JDK10 OpenJ9 build is currently used as the boot JDK. 
# Once JDK10 OpenJ9 releases are available, the following link can be used 
# to download the JDK10 OpenJ9 boot JDK: 
# https://api.adoptopenjdk.net/openjdk10-openj9/releases/x64_linux/latest/binary
RUN cd /root \
  && wget -O bootjdk10.tar.gz https://github.com/AdoptOpenJDK/openjdk10-openj9-nightly/releases/download/jdk-10%2B46-20180509/OpenJDK10-OPENJ9_x64_Linux_20180509.tar.gz \
  && tar -xzf bootjdk10.tar.gz \
  && rm -f bootjdk10.tar.gz \
  && ls | grep -i jdk | xargs -I % sh -c 'mv % bootjdk10'

# Set environment variable JAVA_HOME, and prepend ${JAVA_HOME}/bin to PATH
ENV JAVA_HOME="/root/bootjdk10"
ENV PATH="${JAVA_HOME}/bin:${PATH}"

# The below CFLAG prevents "error: inline function ${FUNCTION_NAME} declared but never defined [-Werror]"
ENV CFLAGS="-fgnu89-inline"

WORKDIR /root
