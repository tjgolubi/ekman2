PROJDIR := $(abspath ..)

EXT := $(PROJDIR)/ext
PFX := $(EXT)/build
LIB := $(PFX)/lib
INC := $(PFX)/include
WRK := $(PFX)/_work

.ONESHELL:
SHELL      := bash
.SHELLOPTS := -ueo pipefail

.PHONY: all clean scour boost libzip shapelib gsl-lite pugixml mp-units

all: boost libzip shapelib gsl-lite pugixml mp-units

clean:
	rm -fr "$(WRK)"

scour: clean
	rm -fr "$(PFX)"

# --------------------------------------------------------------------
# Boost
# --------------------------------------------------------------------

BOOST_SRC   := $(EXT)/boost
BOOST_WRK   := $(WRK)/boost
BOOST_STAGE := $(BOOST_WRK)/stage
BOOST_BUILD := $(BOOST_WRK)/build
BOOST_TGT   := $(LIB)/libboost_program_options.a

boost: $(BOOST_TGT) $(INC)/boost

$(INC)/boost: $(BOOST_SRC)/boost/version.hpp
	mkdir -p "$(INC)"
	rm -fr "$@"
	ln -s "$(BOOST_SRC)/boost" "$@"

$(BOOST_STAGE): $(BOOST_SRC)/b2
	cd "$(BOOST_SRC)"
	./b2 --build-dir="$(BOOST_BUILD)" --stagedir="$(BOOST_STAGE)" \
	  link=static variant=release threading=multi \
	  --with-program_options stage

$(BOOST_TGT): $(BOOST_STAGE)
	mkdir -p "$(LIB)"
	cp -pv $(BOOST_STAGE)/lib/*.a "$(LIB)/"

$(BOOST_SRC)/b2: $(BOOST_SRC)/bootstrap.sh
	cd "$(BOOST_SRC)"
	./bootstrap.sh

# --------------------------------------------------------------------
# libzip
# --------------------------------------------------------------------

LIBZIP_SRC := $(EXT)/libzip
LIBZIP_WRK := $(WRK)/libzip
LIBZIP_TGT := $(INC)/zip.h $(LIB)/libzip.a

LIBZIP_ARGS := -DCMAKE_INSTALL_PREFIX="$(PFX)" \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_DOC=OFF \
  -DBUILD_TOOLS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_REGRESS=OFF \
  -DENABLE_OPENSSL=OFF \
  -DENABLE_GNUTLS=OFF \
  -DENABLE_ZSTD=OFF \
  -DENABLE_BZIP2=OFF \
  -DENABLE_LZMA=OFF

libzip: $(LIBZIP_TGT)

$(LIBZIP_TGT):
	mkdir -p "$(INC)" "$(LIB)" "$(LIBZIP_WRK)"
	cmake -S "$(LIBZIP_SRC)" -B "$(LIBZIP_WRK)" $(LIBZIP_ARGS)
	cmake --build   "$(LIBZIP_WRK)"
	cmake --install "$(LIBZIP_WRK)"

# --------------------------------------------------------------------
# shapelib
# --------------------------------------------------------------------

SHP_SRC := $(EXT)/shapelib
SHP_WRK := $(WRK)/shapelib
SHP_TGT := $(INC)/shapefil.h $(LIB)/libshp.a

SHP_ARGS := -DCMAKE_INSTALL_PREFIX="$(PFX)" \
            -DBUILD_SHARED_LIBS=OFF -DBUILD_APPS=OFF -DBUILD_TESTING=OFF

shapelib: $(SHP_TGT)

$(SHP_WRK)/CMakeCache.txt:
	mkdir -p "$(SHP_WRK)"
	cmake -S "$(SHP_SRC)" -B "$(SHP_WRK)" $(SHP_ARGS)

$(LIB)/libshp.a: $(SHP_WRK)/CMakeCache.txt
	cmake --build   "$(SHP_WRK)"
	cmake --install "$(SHP_WRK)"

$(INC)/shapefil.h: $(LIB)/libshp.a
	@test -f "$@"

# --------------------------------------------------------------------
# gsl-lite
# --------------------------------------------------------------------

GSL_SRC := $(EXT)/gsl-lite
GSL_INC := $(GSL_SRC)/include
GSL_TGT := $(INC)/gsl $(INC)/gsl-lite

gsl-lite: $(GSL_TGT)

$(INC)/gsl: $(GSL_INC)/gsl
	mkdir -p "$(INC)"
	rm -fr "$@"
	ln -s "$<" "$@"

$(INC)/gsl-lite: $(GSL_INC)/gsl-lite
	mkdir -p "$(INC)"
	rm -fr "$@"
	ln -s "$<" "$@"

# --------------------------------------------------------------------
# pugixml
# --------------------------------------------------------------------

PUGI_SRC := $(EXT)/pugixml
PUGI_WRK := $(WRK)/pugixml
PUGI_TGT := $(INC)/pugixml.hpp $(LIB)/libpugixml.a

PUGI_ARGS := -DCMAKE_INSTALL_PREFIX="$(PFX)" \
  -DBUILD_SHARED_LIBS=OFF \
  -DPUGIXML_INSTALL=ON \
  -DPUGIXML_BUILD_TESTS=OFF

pugixml: $(PUGI_TGT)

$(PUGI_TGT):
	mkdir -p "$(INC)" "$(LIB)" "$(PUGI_WRK)"
	cmake -S "$(PUGI_SRC)" -B "$(PUGI_WRK)" $(PUGI_ARGS)
	cmake --build   "$(PUGI_WRK)"
	cmake --install "$(PUGI_WRK)"

MPU_SRC := $(EXT)/mp-units
MPU_TGT := $(INC)/mp-units $(INC)/mp-units/systems

mp-units: $(MPU_TGT)

$(INC)/mp-units: $(MPU_SRC)/src/core/include/mp-units
	mkdir -p $(INC)
	rm -fr "$@"
	cp -pr "$<" "$@"

$(INC)/mp-units/systems: $(MPU_SRC)/src/systems/include/mp-units/systems
	mkdir -p $(INC)
	rm -fr "$@"
	cp -pr "$<" "$@"
