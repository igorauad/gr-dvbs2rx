VERSION_MAJOR="$(cat CMakeLists.txt | grep "set(VERSION_MAJOR" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION_API="$(cat CMakeLists.txt | grep "set(VERSION_API" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION_ABI="$(cat CMakeLists.txt | grep "set(VERSION_ABI" | tr -s ' ' | cut -d' ' -f2 | cut -d')' -f1)"
VERSION=$VERSION_MAJOR"."$VERSION_API"."$VERSION_ABI

if [ "$VERSION" != "$1" ]; then
    echo "Tag $1 does not match CMakeLists.txt version $VERSION" >&2
    exit 1
fi
