#!/bin/bash
# ==============================================================================
# GravityOS — Cross-Compiler Build Script
# ==============================================================================
# x86_64-elf-gcc ve x86_64-elf-binutils'i kaynak koddan derler
# Kullanım: ./scripts/build_cross_compiler.sh
# ==============================================================================

set -e

# Sürümler
BINUTILS_VERSION="2.42"
GCC_VERSION="14.1.0"

# Kurulum dizini
PREFIX="$HOME/opt/cross"
TARGET="x86_64-elf"
PATH="$PREFIX/bin:$PATH"

# İndirme ve derleme dizini
BUILD_BASE="/tmp/gravityos-toolchain"
SRC_DIR="$BUILD_BASE/src"
BUILD_DIR="$BUILD_BASE/build"

echo "============================================"
echo "  GravityOS Cross-Compiler Builder"
echo "============================================"
echo "  Target:   $TARGET"
echo "  Prefix:   $PREFIX"
echo "  Binutils: $BINUTILS_VERSION"
echo "  GCC:      $GCC_VERSION"
echo "============================================"
echo ""

# ---- Bağımlılıkları kontrol et ve kur ----
echo "[1/6] Installing dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq \
    build-essential \
    bison \
    flex \
    libgmp-dev \
    libmpc-dev \
    libmpfr-dev \
    texinfo \
    nasm \
    qemu-system-x86 \
    wget \
    xorriso \
    grub-pc-bin \
    2>/dev/null

echo "[OK] Dependencies installed"

# ---- Dizinleri oluştur ----
echo "[2/6] Creating directories..."
mkdir -p "$PREFIX"
mkdir -p "$SRC_DIR"
mkdir -p "$BUILD_DIR/binutils"
mkdir -p "$BUILD_DIR/gcc"

# ---- Binutils indir ve derle ----
echo "[3/6] Building Binutils $BINUTILS_VERSION..."
cd "$SRC_DIR"

if [ ! -f "binutils-$BINUTILS_VERSION.tar.xz" ]; then
    echo "  Downloading binutils..."
    wget -q "https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.xz"
fi

if [ ! -d "binutils-$BINUTILS_VERSION" ]; then
    echo "  Extracting binutils..."
    tar xf "binutils-$BINUTILS_VERSION.tar.xz"
fi

cd "$BUILD_DIR/binutils"
"$SRC_DIR/binutils-$BINUTILS_VERSION/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --with-sysroot \
    --disable-nls \
    --disable-werror \
    2>&1 | tail -1

echo "  Compiling binutils (this may take a few minutes)..."
make -j$(nproc) 2>&1 | tail -1
make install 2>&1 | tail -1
echo "[OK] Binutils installed"

# ---- GCC indir ve derle ----
echo "[4/6] Building GCC $GCC_VERSION..."
cd "$SRC_DIR"

if [ ! -f "gcc-$GCC_VERSION.tar.xz" ]; then
    echo "  Downloading GCC..."
    wget -q "https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.xz"
fi

if [ ! -d "gcc-$GCC_VERSION" ]; then
    echo "  Extracting GCC..."
    tar xf "gcc-$GCC_VERSION.tar.xz"
fi

cd "$BUILD_DIR/gcc"
"$SRC_DIR/gcc-$GCC_VERSION/configure" \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --enable-languages=c \
    --without-headers \
    2>&1 | tail -1

echo "  Compiling GCC (this will take 10-20 minutes)..."
make -j$(nproc) all-gcc 2>&1 | tail -1
make -j$(nproc) all-target-libgcc 2>&1 | tail -1
make install-gcc 2>&1 | tail -1
make install-target-libgcc 2>&1 | tail -1
echo "[OK] GCC installed"

# ---- PATH'e ekle ----
echo "[5/6] Updating PATH..."
SHELL_RC="$HOME/.bashrc"
if ! grep -q "$PREFIX/bin" "$SHELL_RC" 2>/dev/null; then
    echo "" >> "$SHELL_RC"
    echo "# GravityOS Cross-Compiler" >> "$SHELL_RC"
    echo "export PATH=\"$PREFIX/bin:\$PATH\"" >> "$SHELL_RC"
    echo "  Added to $SHELL_RC"
fi

# ---- Doğrulama ----
echo "[6/6] Verifying installation..."
echo ""

if command -v "$TARGET-gcc" &> /dev/null; then
    echo "  $TARGET-gcc: $($TARGET-gcc --version | head -1)"
else
    echo "  ERROR: $TARGET-gcc not found!"
    echo "  Run: export PATH=\"$PREFIX/bin:\$PATH\""
    exit 1
fi

if command -v "$TARGET-ld" &> /dev/null; then
    echo "  $TARGET-ld:  $($TARGET-ld --version | head -1)"
fi

if command -v nasm &> /dev/null; then
    echo "  nasm:        $(nasm --version)"
fi

if command -v qemu-system-x86_64 &> /dev/null; then
    echo "  qemu:        $(qemu-system-x86_64 --version | head -1)"
fi

echo ""
echo "============================================"
echo "  Cross-compiler setup complete!"
echo "  Run 'source ~/.bashrc' to update PATH"
echo "  Then 'cd gravityos && make run'"
echo "============================================"

# ---- Temizlik (isteğe bağlı) ----
# echo "Cleaning up build files..."
# rm -rf "$BUILD_BASE"
