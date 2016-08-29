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
    DEPS_LIST=`grep "^\(Build\)\?Requires:" *.spec.in | grep -v "%{name}" | tr -s " " | tr "," "\n" | cut -f2 -d " " | grep -v "^satyr" | sort -u | while read br; do if [ "%" = ${br:0:1} ]; then grep "%define $(echo $br | sed -e 's/%{\(.*\)}/\1/')" *.spec.in | tr -s " " | cut -f4 -d" "; else echo $br ;fi ; done | tr "\n" " "`
}

case "$1" in
    "--help"|"-h")
            print_help
            exit 0
        ;;
    "sysdeps")
            build_depslist

            if [ "$2" == "--install" ]; then
                set -x verbose
                sudo dnf install --setopt=strict=0 $DEPS_LIST
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
