#include "imageview.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QResizeEvent>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kZoomStep = 1.25;
constexpr double kMinZoom = 0.02;
constexpr double kMaxZoom = 64.0;
constexpr double kWheelDeltaPerStep = 120.0;

QString firstLocalFile(const QMimeData *mimeData)
{
    if (mimeData == nullptr || !mimeData->hasUrls())
        return {};
    const QList<QUrl> urls = mimeData->urls();
    return urls.isEmpty() || !urls.first().isLocalFile() ? QString() : urls.first().toLocalFile();
}

} // namespace

ImageView::ImageView(QWidget *parent)
    : QGraphicsView(parent)
    , m_scene(new QGraphicsScene(this))
    , m_item(new QGraphicsPixmapItem)
{
    m_item->setTransformationMode(Qt::SmoothTransformation);
    m_scene->addItem(m_item);
    setScene(m_scene);

    setBackgroundBrush(Qt::black);
    setFrameShape(QFrame::NoFrame);
    setAlignment(Qt::AlignCenter);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setDragMode(QGraphicsView::ScrollHandDrag);
    // O botão direito é usado para ajustar a janela, não para menu de contexto.
    setContextMenuPolicy(Qt::NoContextMenu);
    setAcceptDrops(true);
}

void ImageView::setImage(const QImage &image, double pixelAspectRatio)
{
    m_item->setPixmap(QPixmap::fromImage(image));
    m_item->setTransform(QTransform::fromScale(pixelAspectRatio, 1.0));
    m_scene->setSceneRect(m_item->sceneBoundingRect());
    m_hasImage = !image.isNull();
    fitToWindow();
    viewport()->update();
}

void ImageView::updateImage(const QImage &image)
{
    m_item->setPixmap(QPixmap::fromImage(image));
}

void ImageView::clear()
{
    m_item->setPixmap(QPixmap());
    m_hasImage = false;
    resetTransform();
    viewport()->update();
}

bool ImageView::hasImage() const
{
    return m_hasImage;
}

double ImageView::zoomFactor() const
{
    return transform().m22();
}

void ImageView::fitToWindow()
{
    m_fitToWindow = true;
    if (!m_hasImage)
        return;
    fitInView(m_item, Qt::KeepAspectRatio);
    emit zoomChanged(zoomFactor());
}

void ImageView::zoomToActualSize()
{
    if (!m_hasImage)
        return;
    m_fitToWindow = false;
    resetTransform();
    emit zoomChanged(zoomFactor());
}

void ImageView::zoomIn()
{
    zoomBy(kZoomStep);
}

void ImageView::zoomOut()
{
    zoomBy(1.0 / kZoomStep);
}

void ImageView::zoomBy(double factor)
{
    if (!m_hasImage)
        return;
    const double current = zoomFactor();
    const double target = std::clamp(current * factor, kMinZoom, kMaxZoom);
    m_fitToWindow = false;
    scale(target / current, target / current);
    emit zoomChanged(zoomFactor());
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    if (m_fitToWindow)
        fitToWindow();
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    if (!m_hasImage) {
        QGraphicsView::wheelEvent(event);
        return;
    }
    const double steps = event->angleDelta().y() / kWheelDeltaPerStep;
    if (steps != 0.0)
        zoomBy(std::pow(kZoomStep, steps));
    event->accept();
}

void ImageView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton && m_hasImage) {
        m_windowing = true;
        m_lastMousePos = event->position().toPoint();
        m_savedCursor = viewport()->cursor();
        viewport()->setCursor(Qt::SizeAllCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_windowing) {
        const QPoint position = event->position().toPoint();
        const QPoint delta = position - m_lastMousePos;
        m_lastMousePos = position;
        if (!delta.isNull())
            emit windowDragged(delta.x(), delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_windowing && event->button() == Qt::RightButton) {
        m_windowing = false;
        viewport()->setCursor(m_savedCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_hasImage) {
        fitToWindow();
        event->accept();
        return;
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}

void ImageView::drawForeground(QPainter *painter, const QRectF &rect)
{
    QGraphicsView::drawForeground(painter, rect);
    if (m_hasImage)
        return;

    painter->save();
    painter->resetTransform();
    painter->setPen(QColor(170, 170, 170));
    painter->drawText(viewport()->rect().adjusted(24, 24, -24, -24),
                      Qt::AlignCenter | Qt::TextWordWrap,
                      QStringLiteral("Abra um arquivo DICOM pelo menu Arquivo > Abrir (Ctrl+O)\n"
                                     "ou arraste-o para esta área."));
    painter->restore();
}

void ImageView::dragEnterEvent(QDragEnterEvent *event)
{
    if (firstLocalFile(event->mimeData()).isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void ImageView::dragMoveEvent(QDragMoveEvent *event)
{
    if (firstLocalFile(event->mimeData()).isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void ImageView::dropEvent(QDropEvent *event)
{
    const QString filePath = firstLocalFile(event->mimeData());
    if (filePath.isEmpty()) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
    emit fileDropped(filePath);
}
