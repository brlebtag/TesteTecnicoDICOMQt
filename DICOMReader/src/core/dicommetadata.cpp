#include "dicommetadata.h"

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcmetinf.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dctag.h>

#include <QLocale>
#include <QStringDecoder>

#include <cstring>

namespace dicomreader {
namespace {

constexpr qsizetype kMaxValueLength = 256;
constexpr Uint32 kUndefinedLength = 0xFFFFFFFFu;

/// VRs cujo conteúdo é binário (ou listas enormes de números) e que são
/// exibidos apenas resumidos.
bool isBinaryVr(DcmEVR vr)
{
    switch (vr) {
    case EVR_OB:
    case EVR_OD:
    case EVR_OF:
    case EVR_OL:
    case EVR_OV:
    case EVR_OW:
    case EVR_UN:
    case EVR_ox:
    case EVR_px:
    case EVR_lt:
    case EVR_pixelSQ:
    case EVR_PixelData:
    case EVR_OverlayData:
        return true;
    default:
        return false;
    }
}

/// Os textos já foram convertidos para UTF-8 quando o arquivo informa o
/// conjunto de caracteres; sem essa informação, cai para Latin-1.
QString decodeText(const OFString &text)
{
    const QByteArrayView bytes(text.c_str(), static_cast<qsizetype>(text.size()));
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString decoded = decoder.decode(bytes);
    if (decoder.hasError())
        decoded = QString::fromLatin1(bytes);
    return decoded;
}

QString formatByteCount(Uint32 length)
{
    if (length == kUndefinedLength)
        return QStringLiteral("<dados encapsulados>");
    return QStringLiteral("<%1 bytes>").arg(QLocale().toString(static_cast<qulonglong>(length)));
}

QString formatValue(DcmElement &element)
{
    if (isBinaryVr(element.ident()))
        return formatByteCount(element.getLengthField());

    OFString value;
    if (element.getOFStringArray(value).bad())
        return {};

    QString text = decodeText(value).simplified();
    if (text.size() > kMaxValueLength)
        text = text.left(kMaxValueLength) + QStringLiteral("…");
    return text;
}

QString formatName(DcmTag &tag)
{
    const char *name = tag.getTagName();
    if (name != nullptr && std::strcmp(name, DcmTag_ERROR_TagName) != 0)
        return QString::fromLatin1(name);
    return tag.isPrivate() ? QStringLiteral("(atributo privado)")
                           : QStringLiteral("(atributo desconhecido)");
}

QString formatItemCount(unsigned long count)
{
    return count == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 itens").arg(count);
}

void appendItem(DcmItem &item, int depth, QList<MetadataEntry> &entries)
{
    const unsigned long count = item.card();
    for (unsigned long i = 0; i < count; ++i) {
        DcmElement *element = item.getElement(i);
        if (element == nullptr)
            continue;

        DcmTag tag(element->getTag());
        MetadataEntry entry;
        entry.tag = QString::fromLatin1(tag.toString().c_str());
        entry.vr = QString::fromLatin1(tag.getVRName());
        entry.name = formatName(tag);
        entry.depth = depth;

        if (element->ident() != EVR_SQ) {
            entry.value = formatValue(*element);
            entries.append(entry);
            continue;
        }

        auto *sequence = static_cast<DcmSequenceOfItems *>(element);
        const unsigned long itemCount = sequence->card();
        entry.value = formatItemCount(itemCount);
        entries.append(entry);

        for (unsigned long j = 0; j < itemCount; ++j) {
            MetadataEntry marker;
            marker.name = QStringLiteral("Item %1").arg(j + 1);
            marker.depth = depth + 1;
            entries.append(marker);
            if (DcmItem *child = sequence->getItem(j))
                appendItem(*child, depth + 2, entries);
        }
    }
}

} // namespace

QList<MetadataEntry> extractMetadata(DcmFileFormat &fileFormat)
{
    QList<MetadataEntry> entries;
    if (DcmMetaInfo *metaInfo = fileFormat.getMetaInfo())
        appendItem(*metaInfo, 0, entries);
    if (DcmDataset *dataset = fileFormat.getDataset())
        appendItem(*dataset, 0, entries);
    return entries;
}

} // namespace dicomreader
