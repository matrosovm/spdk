FROM ubuntu:latest

ADD ../.. spdk
RUN apt-get update && apt-get install -y libfuse3-dev
RUN spdk/scripts/pkgdep.sh

CMD ["bin/bash"]