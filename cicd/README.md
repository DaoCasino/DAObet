## Building, packaging and deploying
For build, packaging and push to Github Releases you can execute script ./cicd/run with some options.

```shell
./cicd/run
Usage: ./cicd/run <subcommand> [options]
Subcommands:
    build   build toolchain and project
    push    Push toolchain and project artifacts to DockerHub and GitHub Releases

For help with each subcommand run:
./cicd/run <subcommand> -h
```

Examples:
Building tools, product and create deb package
```shell
./cicd/run build -d ubuntu -t -p haya
```
If you want to use the external build tools docker images, you can do following:
```shell
docker pull mixbytes/haya-build-tools-ubuntu:latest
docker tag mixbytes/haya-build-tools-ubuntu:latest haya-build-tools-ubuntu
./cicd/run -d ubuntu -p haya
```
Create docker image with installed deb-package:
```shell
./cicd/run build -i -p haya
```
Push created packages to Github Releases:
```shell
cat ~/.netrc
machine api.github.com
  login user
  password api_token
machine uploads.github.com
  login user
  password api_token
```
```shell
./cicd/run push -p github -r mixbytes/haya haya
```

For more details, see ./cicd/run build -h
```shell
Usage: ./cicd/run build [options] {projact_name}
Options:
    -t   build toolchain for specific platform (-d option required)
    -p   build project and create OS package (-d option required)
    -i   build Docker image
    -s   start Docker container and run shell in specific platform (-d option required)
    -d   target platform {ubuntu,centos}

Example:
# Build toolchain and project package for Ubuntu
./cicd/run build -d ubuntu -t -p haya
```

