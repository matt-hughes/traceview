SRC=TraceViewIcon.png
DEST=./TraceView.iconset
mkdir -p $DEST
sips -z 16 16     $SRC --out $DEST/icon_16x16.png
sips -z 32 32     $SRC --out $DEST/icon_16x16@2x.png
sips -z 32 32     $SRC --out $DEST/icon_32x32.png
sips -z 64 64     $SRC --out $DEST/icon_32x32@2x.png
sips -z 128 128   $SRC --out $DEST/icon_128x128.png
sips -z 256 256   $SRC --out $DEST/icon_128x128@2x.png
sips -z 256 256   $SRC --out $DEST/icon_256x256.png
sips -z 512 512   $SRC --out $DEST/icon_256x256@2x.png
sips -z 512 512   $SRC --out $DEST/icon_512x512.png
sips -z 1024 1024 $SRC --out $DEST/icon_512x512@2x.png
iconutil -c icns $DEST
rm -rf $DEST
