# build with
# docker build --rm --build-arg USER_ID=$(id -u) --build-arg GROUP_ID=$(id -g) \
#   --build-arg BRIGHTSIGN_OS_VERSION=9.1.52 -t bsoe-build .

# run with
# docker run -it --rm \
#   -v bsoe-source:/home/builder/bsoe \
#   -v $(pwd)/bsoe-recipes:/home/builder/patches:ro \
#   -v $(pwd)/srv:/srv \
#   bsoe-build


FROM debian:bullseye

# Arguments for user configuration
ARG USER_ID=1000
ARG GROUP_ID=1000
ARG USERNAME=builder

# BrightSign OS version configuration
ARG BRIGHTSIGN_OS_VERSION=9.1.52

# Configure locales and enable multiarch in a single layer
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y \
        locales \
        avahi-daemon \
        build-essential \
        diffstat \
        texi2html \
        cvs \
        subversion \
        gawk \
        chrpath \
        screen \
        patchutils \
        ruby \
        libgl1-mesa-dev \
        libglu1-mesa-dev \
        uuid-dev \
        libsdl1.2-dev \
        lzop \
        autofs \
        zip \
        curl \
        tftpd-hpa \
        nfs-server \
        ed \
        dbus-x11 \
        time \
        texinfo \
        bc \
        libpcre16-3 \
        rsync \
        wget \
        pigz \
        moreutils \
        libc6:i386 \
        libstdc++6:i386 \
        libglib2.0-0:i386 \
        g++-multilib \
        libxcb-icccm4 \
        libxcb-image0 \
        libxcb-shape0 \
        libxcb-keysyms1 \
        libxcb-render-util0 \
        libxcb-xinerama0 \
        libxcb-xkb1 \
        libxkbcommon-x11-0 \
        libxkbcommon0 \
        python3-requests \
        git \
        apticron \
        etckeeper && \
    sed -i -e 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen && \
    echo 'LANG="en_US.UTF-8"' > /etc/default/locale && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    echo "dash dash/sh boolean false" | debconf-set-selections && \
    dpkg-reconfigure --frontend=noninteractive dash && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update && \
    apt-get install -y \
        lsb-release \
        cpio \
        sudo \
        file

# Create user with the same UID/GID as the host user
RUN groupadd -g ${GROUP_ID} ${USERNAME} && \
    useradd -m -u ${USER_ID} -g ${USERNAME} -s /bin/bash ${USERNAME} && \
    echo "${USERNAME} ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Set environment variables
ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    BRIGHTSIGN_OS_VERSION=${BRIGHTSIGN_OS_VERSION}

# Create and set proper ownership of the working directory
RUN mkdir -p /home/${USERNAME}/bsoe && \
    chown -R ${USERNAME}:${USERNAME} /home/${USERNAME}

# Set working directory
WORKDIR /home/${USERNAME}/bsoe

# Switch to non-root user early to set proper ownership
USER ${USERNAME}

# Download and extract BrightSign OS source during build
RUN cd /home/${USERNAME}/bsoe && \
    echo "Downloading BrightSign OS source v${BRIGHTSIGN_OS_VERSION}..." && \
    echo "Downloading dl archive (~20GB)..." && \
    wget --progress=dot:giga \
        "https://brightsignbiz.s3.amazonaws.com/firmware/opensource/$(echo ${BRIGHTSIGN_OS_VERSION} | cut -d'.' -f1-2)/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz" && \
    echo "Downloading oe archive (~169MB)..." && \
    wget --progress=dot:mega \
        "https://brightsignbiz.s3.amazonaws.com/firmware/opensource/$(echo ${BRIGHTSIGN_OS_VERSION} | cut -d'.' -f1-2)/${BRIGHTSIGN_OS_VERSION}/brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz" && \
    echo "Extracting source archives..." && \
    tar -xzf "brightsign-${BRIGHTSIGN_OS_VERSION}-src-dl.tar.gz" && \
    tar -xzf "brightsign-${BRIGHTSIGN_OS_VERSION}-src-oe.tar.gz" && \
    echo "Cleaning up downloaded archives..." && \
    rm -f *.tar.gz && \
    echo "BrightSign OS source v${BRIGHTSIGN_OS_VERSION} ready"

# Copy patch script (will be called by build script when needed)
COPY setup-patches.sh /usr/local/bin/setup-patches.sh
RUN sudo chmod +x /usr/local/bin/setup-patches.sh

# Default command - clean container with no automatic execution
CMD ["/bin/bash"]