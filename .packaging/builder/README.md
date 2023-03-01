# Package Builder Containers

## RPM

Build the builder image:

```
docker build -t dvbs2rx_rpm_builder -f rpm_builder.docker .
```

Launch the builder container:

```
docker run --rm -it dvbs2rx_rpm_builder
```

Build the package inside the container:
```
git clone --recursive https://github.com/igorauad/gr-dvbs2rx/ && cd gr-dvbs2rx/
.packaging/scripts/pkg-rpm.sh
```

## Debian

Build the builder image:

```
docker build -t dvbs2rx_deb_builder -f deb_builder.docker .
```

Launch the builder container:

```
docker run --rm -it dvbs2rx_deb_builder
```

Build the package inside the container:
```
git clone --recursive https://github.com/igorauad/gr-dvbs2rx/ && cd gr-dvbs2rx/
.packaging/scripts/pkg-debian.sh
```