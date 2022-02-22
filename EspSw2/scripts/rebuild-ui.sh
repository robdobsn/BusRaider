BUILDREV=${1:-BusRaider}
IPORHOSTNAME=${2:-}
python3 ./scripts/UITemplater.py buildConfigs/$BUILDREV/WebUI buildConfigs/$BUILDREV/FSImage
curl -F "file=@./buildConfigs/$BUILDREV/FSImage/index.html.gz" "http://$2/api/fileupload"
