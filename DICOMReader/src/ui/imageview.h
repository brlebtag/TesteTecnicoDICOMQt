#ifndef DICOMREADER_UI_IMAGEVIEW_H
#define DICOMREADER_UI_IMAGEVIEW_H

#include <QCursor>
#include <QGraphicsView>
#include <QImage>
#include <QPoint>

class QGraphicsPixmapItem;
class QGraphicsScene;

/// Área de visualização da imagem.
///
/// - Ajusta a imagem à janela (padrão), com zoom pela roda do mouse e pan
///   arrastando com o botão esquerdo; duplo clique volta a ajustar.
/// - Corrige pixels não quadrados com uma transformação de exibição, sem
///   alterar os dados da imagem.
/// - Arrastar com o botão direito emite windowDragged() para o ajuste de
///   janela (WL/WW).
/// - Aceita arquivos arrastados do sistema (fileDropped()).
class ImageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);

    /// Exibe uma nova imagem e a ajusta à janela.
    /// @param pixelAspectRatio largura/altura de um pixel (1.0 = quadrado).
    void setImage(const QImage &image, double pixelAspectRatio);
    /// Troca o conteúdo mantendo zoom e posição (mudança de janela ou de frame).
    void updateImage(const QImage &image);
    void clear();

    bool hasImage() const;
    /// Escala atual: pixels de tela por linha da imagem (1.0 = tamanho real).
    double zoomFactor() const;

public slots:
    void fitToWindow();
    void zoomToActualSize();
    void zoomIn();
    void zoomOut();

signals:
    void windowDragged(int dx, int dy);
    void zoomChanged(double factor);
    void fileDropped(const QString &filePath);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void drawForeground(QPainter *painter, const QRectF &rect) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void zoomBy(double factor);

    QGraphicsScene *m_scene = nullptr;
    QGraphicsPixmapItem *m_item = nullptr;
    bool m_hasImage = false;
    bool m_fitToWindow = true;
    bool m_windowing = false;
    QPoint m_lastMousePos;
    QCursor m_savedCursor;
};

#endif // DICOMREADER_UI_IMAGEVIEW_H
