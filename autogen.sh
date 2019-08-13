#! /bin/sh

print_help()
{
cat << EOH
Prepares the source tree for configuration

Usage:
  autogen.sh [sysdeps [--install]]

Options:

  sysdeps          prints out all dependencies
    --install      install all dependencies ('sudo yum install \$DEPS')

EOH
}

build_depslist()
{
    PACKAGE=$1
    TEMPFILE=$(mktemp -u --suffix=.spec)
    sed 's/@@SATYR_VERSION@@/1/' < $PACKAGE.spec.in | sed 's/@.*@//' > $TEMPFILE
    rpmspec -P $TEMPFILE | grep "^\(Build\)\?Requires:" | \
        tr -s " " | tr "," "\n" | cut -f2- -d " " | \
        grep -v "\(^\|python[23]-\)"$PACKAGE | sort -u | \
        sed -E -e 's/^(.*) (.*)$/"\1 \2"/' -e 's|([[:alpha:]]+\(.+\))|"\1"|' | \
        tr \" \'
    rm $TEMPFILE
}

case "$1" in
    "--help"|"-h")
            print_help
            exit 0
        ;;
    "sysdeps")
            DEPS_LIST=$(build_depslist satyr)

            if [ "$2" == "--install" ]; then
                set -x verbose
                eval sudo dnf install --setopt=strict=0 $DEPS_LIST
                set +x verbose
            else
                echo $DEPS_LIST
            fi
            exit 0
        ;;
    *)
            echo "Running gen-version"
            ./gen-version

            echo "Running autoreconf"
            autoreconf --install --force

            echo "Running configure"
            ./configure "$@"
        ;;
esac
