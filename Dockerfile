FROM scratch
ADD ubuntu-focal-core-cloudimg-amd64-root.tar.gz /

# a few minor docker-specific tweaks
# see https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap
RUN set -xe \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L40-L48
	&& echo '#!/bin/sh' > /usr/sbin/policy-rc.d \
	&& echo 'exit 101' >> /usr/sbin/policy-rc.d \
	&& chmod +x /usr/sbin/policy-rc.d \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L54-L56
	&& dpkg-divert --local --rename --add /sbin/initctl \
	&& cp -a /usr/sbin/policy-rc.d /sbin/initctl \
	&& sed -i 's/^exit.*/exit 0/' /sbin/initctl \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L71-L78
	&& echo 'force-unsafe-io' > /etc/dpkg/dpkg.cfg.d/docker-apt-speedup \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L85-L105
	&& echo 'DPkg::Post-Invoke { "rm -f /var/cache/apt/archives/*.deb /var/cache/apt/archives/partial/*.deb /var/cache/apt/*.bin || true"; };' > /etc/apt/apt.conf.d/docker-clean \
	&& echo 'APT::Update::Post-Invoke { "rm -f /var/cache/apt/archives/*.deb /var/cache/apt/archives/partial/*.deb /var/cache/apt/*.bin || true"; };' >> /etc/apt/apt.conf.d/docker-clean \
	&& echo 'Dir::Cache::pkgcache ""; Dir::Cache::srcpkgcache "";' >> /etc/apt/apt.conf.d/docker-clean \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L109-L115
	&& echo 'Acquire::Languages "none";' > /etc/apt/apt.conf.d/docker-no-languages \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L118-L130
	&& echo 'Acquire::GzipIndexes "true"; Acquire::CompressionTypes::Order:: "gz";' > /etc/apt/apt.conf.d/docker-gzip-indexes \
	\
# https://github.com/docker/docker/blob/9a9fc01af8fb5d98b8eec0740716226fadb3735c/contrib/mkimage/debootstrap#L134-L151
	&& echo 'Apt::AutoRemove::SuggestsImportant "false";' > /etc/apt/apt.conf.d/docker-autoremove-suggests

# verify that the APT lists files do not exist
RUN [ -z "$(apt-get indextargets)" ]
# (see https://bugs.launchpad.net/cloud-images/+bug/1699913)

# make systemd-detect-virt return "docker"
# See: https://github.com/systemd/systemd/blob/aa0c34279ee40bce2f9681b496922dedbadfca19/src/basic/virt.c#L434
RUN mkdir -p /run/systemd && echo 'docker' > /run/systemd/container

# Lumin append the followings:
# ----------------------------------------------------------------------------------------------------------------------

# install build tools
RUN apt update
RUN apt -y install libssl-dev 
RUN apt -y install gcc g++
RUN apt -y install make
RUN apt -y install libreadline-gplv2-dev libncursesw5-dev libsqlite3-dev libgdbm-dev libc6-dev libbz2-dev zlib1g-dev

# configure Python environment
WORKDIR /usr/lib
ADD Python-3.7.9.tgz .
WORKDIR /usr/lib/Python-3.7.9
RUN ./configure --prefix=/usr/lib/python3.7.9
RUN make
RUN make install
RUN sed -i "$ a export PATH=/usr/lib/python3.7.9/bin:\$PATH" /root/.bashrc
WORKDIR /usr/lib/python3.7.9/bin
RUN ln -s python3 python
RUN ln -s pip3 pip

# configure qemu simulator 
WORKDIR /usr/lib
ADD qemu-4.1.1.tar.xz /usr/lib
WORKDIR /usr/lib/qemu-4.1.1
ENV DEBIAN_FRONTEND=noninteractive
RUN apt -y install pkg-config
RUN apt -y install libglib2.0-dev libpixman-1-dev
RUN ./configure --target-list=riscv32-softmmu,riscv64-softmmu
RUN make -j
RUN sed -i "$ a export PATH=/usr/lib/qemu-4.1.1/riscv32-softmmu:/usr/lib/qemu-4.1.1/riscv64-softmmu:\$PATH" /root/.bashrc

# configure risc-v toolchains
WORKDIR /usr/lib
ADD riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14.tar.gz .
RUN mv riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-linux-ubuntu14 riscv64-unknown-elf-toolchain
RUN sed -i "$ a export PATH=/usr/lib/riscv64-unknown-elf-toolchain/bin:\$PATH" /root/.bashrc

# configure K210 environment
RUN apt -y install python3-serial
RUN /usr/lib/python3.7.9/bin/pip3 install pyserial

# configure usable tools
RUN apt -y install git

WORKDIR /home/riscv64-ucore

CMD ["/bin/bash"]