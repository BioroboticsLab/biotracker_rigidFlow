#include "BeeDance.h"

#include <QApplication>
#include <QIntValidator>
#include <QPushButton>
#include <QPainter>
#include <QColorDialog>
#include <QDateTime>
#include <QRadioButton>
#include <QFormLayout>

#include <QFileDialog>
#include <biotracker/TrackingAlgorithm.h>
#include <biotracker/Registry.h>

#define RS_NOT_SET 0
#define RS_SET 1
#define RS_INITIALIZE 2
#define RS_SCALE 3
#define RS_DRAG 4
#define RS_ROTATE 5

#define BOX_COLOR 20, 30, 255
#define BOX_COLOR_INACTIVE 190, 190, 190
#define PAST_TRACK_COLOR 70, 240, 15
#define FUTURE_TRACK_COLOR 35, 120, 8

using namespace BioTracker::Core;

extern "C" {
    #ifdef _WIN32
    void __declspec(dllexport) registerTracker() {
    #else
    void registerTracker() {
    #endif
        BioTracker::Core::Registry::getInstance().registerTrackerType<BeeDanceTracker>("BeeDance");
    }
}

BeeDanceTracker::BeeDanceTracker(Settings &settings):
    TrackingAlgorithm(settings),
    m_futuresteps(10),
    m_noncorrectionsteps(10),
    m_correction_enabled(false),
    m_features(1000),
    m_automatictracking(true),
    m_updatefeatures(false),
    m_fixedratio(true),
    m_path_showing(false),
    m_path_changed(false),
    m_ratio(2.5),
    m_rectstat(RS_NOT_SET),
    m_of_tracker(new OverlapOFTracker()),
    m_cto(0),
    m_start_of_tracking(true),
    m_mouseOverPath(-1),
    m_futurestepsEdit(new QLineEdit(getToolsWidget())),
    m_noncorrectionstepsEdit(new QLineEdit(getToolsWidget())),
    m_enable_correction(new QCheckBox(getToolsWidget())),
    m_featuresEdit(new QLineEdit(getToolsWidget())),
    m_fixedratioEdit(new QCheckBox(getToolsWidget()))
{
    // initialize gui
    auto ui = getToolsWidget();
    auto layout = new QFormLayout();

    m_futurestepsEdit->setText(QString::number(m_futuresteps));
    layout->addRow("Future Steps", m_futurestepsEdit);

    m_noncorrectionstepsEdit->setText(QString::number(m_noncorrectionsteps));
    m_noncorrectionstepsEdit->setDisabled(!m_correction_enabled);
    layout->addRow("Non-Correction Steps", m_noncorrectionstepsEdit);

    m_enable_correction->setChecked(m_correction_enabled);
    QObject::connect(m_enable_correction, &QCheckBox::stateChanged,
                     this, &BeeDanceTracker::enableCorrection);
    layout->addRow("Enable Correction", m_enable_correction);

    m_featuresEdit->setText(QString::number(m_features));
    layout->addRow("Number of Features", m_featuresEdit);

    auto *paramBut = new QPushButton("Change Parameters");
    QObject::connect(paramBut, &QPushButton::clicked,
                     this, &BeeDanceTracker::changeParams);
    layout->addRow(paramBut);

    auto *satracking = new QRadioButton();
    satracking->setChecked(!m_automatictracking);
    QObject::connect(satracking, &QRadioButton::clicked,
                     this, &BeeDanceTracker::switchMode);
    layout->addRow("Semi-automatic tracking", satracking);

    auto *atracking = new QRadioButton();
    atracking->setChecked(m_automatictracking);
    QObject::connect(atracking, &QRadioButton::clicked,
                     this, &BeeDanceTracker::switchMode);
    layout->addRow("Automatic tracking", atracking);

    m_fixedratioEdit->setChecked(m_fixedratio);
    QObject::connect(m_fixedratioEdit, &QCheckBox::stateChanged,
                     this, &BeeDanceTracker::fixRatio);
    layout->addRow("Fixed Aspect Ratio", m_fixedratioEdit);

    auto *showPath = new QCheckBox();
    showPath->setChecked(m_path_showing);
    QObject::connect(showPath, &QCheckBox::stateChanged,
                     this, &BeeDanceTracker::showPath);
    layout->addRow("Show Path", showPath);

    ui->setLayout(layout);
}

void BeeDanceTracker::track(size_t frame, const cv::Mat &imgOriginal) {
    m_currentFrame = frame;

    cv::Mat imgCopy = imgOriginal.clone();

    if(imgCopy.empty()) return;

    if(m_cto >= static_cast<int>(m_trackedObjects.size())) return;

    // skipped through video
    if(!m_trackedObjects[m_cto].hasValuesAtFrame(frame) && !m_trackedObjects[m_cto].hasValuesAtFrame(frame - 1)){
        return;
    }

    if (m_start_of_tracking) {
        m_start_of_tracking = false;
    }
    if (!m_of_tracker->isInitialized()) {
        //semi-automatic or automatic tracking
        if (!m_automatictracking) {
            reinterpret_cast<SingleOFTracker*>(m_of_tracker)->configure(m_features);
            reinterpret_cast<SingleOFTracker*>(m_of_tracker)->init(imgCopy);
        }
        else {
            reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->configure(m_futuresteps, m_noncorrectionsteps, m_features, m_correction_enabled);
            reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->init(imgCopy);
        }
    }
    //copy BeeBox from previous frame
    if (!m_trackedObjects[m_cto].hasValuesAtFrame(frame)) {
        auto o = std::make_shared<BeeBox>(m_trackedObjects[m_cto].get<BeeBox>(frame - 1));
        m_trackedObjects[m_cto].push_back(o);
    } else if (m_trackedObjects[m_cto].hasValuesAtFrame(frame - 1) && !m_path_changed) {
        auto o = std::make_shared<BeeBox>(m_trackedObjects[m_cto].get<BeeBox>(frame - 1));
        m_trackedObjects[m_cto].add(frame, o);
    }
    m_path_changed = false;
    //calculate movement for next step
    m_of_tracker->next(imgCopy, *m_trackedObjects[m_cto].get<BeeBox>(frame));
}

void BeeDanceTracker::paint(size_t , ProxyMat & , const TrackingAlgorithm::View &) { }

void BeeDanceTracker::paintOverlay(size_t frame, QPainter *painter, const View &) {
    m_currentFrame = frame;

    if (m_path_showing) {
        drawPath(painter);
    }

    if (m_rectstat >= RS_SET) {
        drawRectangle(painter, static_cast<int>(frame));
    }

}

// =========== I O = H A N D L I N G ============


// ============== Keyboard ==================

void BeeDanceTracker::keyPressEvent(QKeyEvent *) { }

// ============== Mouse ==================

void BeeDanceTracker::mousePressEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (getVideoMode() != GuiParam::VideoMode::Paused) return;
    //check if clicked on path
    m_mouseOverPath = -1;

    if(m_cto < static_cast<int>(m_trackedObjects.size())) {
        auto currentPath = m_trackedObjects[m_cto];
        for (size_t frame = 0; frame < currentPath.getLastFrameNumber().get() + 1; frame++) {
            if (currentPath.hasValuesAtFrame(frame)) {
                BeeBox point = currentPath.get<BeeBox>(frame);
                if (abs(e->x() - static_cast<int>(point.x)) <= 2 && abs(e->y() - static_cast<int>(point.y)) <= 2) {
                    m_mouseOverPath = m_cto;
                    break;
                }
            }
        }
    }

    //check if left button is clicked
    if (e->button() == Qt::LeftButton) {
        // add new BeeBox
        if (e->modifiers() == Qt::ControlModifier) {
            m_cto = m_trackedObjects.size();

            auto bb = std::make_shared<BeeBox>();
            TrackedObject o(m_cto);
            o.add(m_currentFrame, bb);
            m_trackedObjects.push_back(o);

            bb->x = e->x();
            bb->y = e->y();
            m_rectstat = RS_INITIALIZE;
            bb->phi = 0;
        }

        if (m_rectstat == RS_SET) {
            int close = 8;

            //check if mouse click happened inside any rectangle
            bool in = false;
            for (auto o : m_trackedObjects) {
                if (o.hasValuesAtFrame(m_currentFrame)) {
                    bool tmpIn = true;
                    std::vector<cv::Point2i> pts = o.get<BeeBox>(m_currentFrame)->getCornerPoints();
                    for (int i = 0; i < 4; i++) {
                        cv::Point2i pd = pts[(i + 1) % 4] - pts[i];
                        pd = cv::Point2i(-pd.y, pd.x);
                        cv::Point md = cv::Point2i(e->x() - pts[i].x, e->y() - pts[i].y);
                        if (pd.dot(md) > 0) {
                            tmpIn = false;
                        }
                    }
                    if (tmpIn) {
                        in = true;
                        m_cto = o.getId();
                        break;
                    }
                }
            }
            updatePoints(static_cast<int>(m_currentFrame));
            //scale mode
            for (int i = 0; i < 4; i++) {
                if (abs(e->x() - m_pts[i].x) <= close && abs(e->y() - m_pts[i].y) <= close) {
                    m_rectstat = RS_SCALE;
                }
            }
            //drag mode
            if (m_rectstat == RS_SET && in && m_mouseOverPath == -1) {
                m_mdx = e->x();
                m_mdy = e->y();
                m_rectstat = RS_DRAG;
            }
        }
    }
    //deletes a previously selected path that is right-clicked
    else if (e->button() == Qt::RightButton) {
        if(m_cto >= static_cast<int>(m_trackedObjects.size()) || !m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) return;

        if (m_mouseOverPath == m_cto && m_path_showing) {
            auto bb = std::make_shared<BeeBox>(m_trackedObjects[m_cto].get<BeeBox>(m_currentFrame));
            TrackedObject o(m_cto);
            o.add(m_currentFrame, bb);
            m_trackedObjects[m_cto] = o;

            m_mouseOverPath = -1;
        } else {
            updatePoints(static_cast<int>(m_currentFrame));

            //check if mouse click happened inside the rectangle
            bool in = true;
            for (int i = 0; i < 4; i++){
                cv::Point2i pd = m_pts[(i + 1) % 4] - m_pts[i];
                pd = cv::Point2i(-pd.y, pd.x);
                cv::Point md = cv::Point2i(e->x() - m_pts[i].x, e->y() - m_pts[i].y);
                if (pd.dot(md) > 0){
                    in = false;
                    break;
                }
            }
            if (m_rectstat == RS_SET && in && m_mouseOverPath == -1){
                //rotate mode
                m_last_rotation_point = cv::Point2i(static_cast<int>(e->x()), static_cast<int>(e->y()));
                m_rectstat = RS_ROTATE;
            }
        }
        Q_EMIT update();
    }
}

void BeeDanceTracker::mouseMoveEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (getVideoMode() != GuiParam::VideoMode::Paused) return;
    if(m_cto >= static_cast<int>(m_trackedObjects.size()) || !m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) return;
    auto currentBeeBox = m_trackedObjects[m_cto].get<BeeBox>(m_currentFrame);

    //what we do when we are scaling the bounding box
    if (m_rectstat == RS_INITIALIZE || m_rectstat == RS_SCALE) {
        float p = static_cast<float>(currentBeeBox->phi * 3.1415 / 180);
        float h = e->x() - currentBeeBox->x;
        float w = e->y() - currentBeeBox->y;
        int x = static_cast<int>(currentBeeBox->x + sin(-p) * h - cos(-p) * w);
        int y = static_cast<int>(currentBeeBox->y + cos(-p) * h + sin(-p) * w);
        currentBeeBox->h = 2 * abs(x - static_cast<int>(currentBeeBox->x));

        if (m_fixedratio) currentBeeBox->w = currentBeeBox->h / m_ratio;
        else currentBeeBox->w = 2 * abs(y - static_cast<int>(currentBeeBox->y));
        m_path_changed = true;

        Q_EMIT update();
    }
        //what we do when we are dragging the bounding box
    else if (m_rectstat == RS_DRAG) {
        currentBeeBox->x += e->x() - m_mdx;
        currentBeeBox->y += e->y() - m_mdy;
        m_mdx = e->x();
        m_mdy = e->y();
        m_path_changed = true;
    }
        //what we do when we are rotating the bounding box
    else if (m_rectstat == RS_ROTATE) {
        auto tmpLR = cv::Point2i(static_cast<int>(m_last_rotation_point.x - currentBeeBox->x),
                            static_cast<int>(m_last_rotation_point.y - currentBeeBox->y));
        auto tmpE = cv::Point2i(static_cast<int>(e->x() - currentBeeBox->x), static_cast<int>(e->y() - currentBeeBox->y));
        float phiTemp = static_cast<float>(atan2(tmpLR.y, tmpLR.x) * 180 / M_PI) - static_cast<float>(atan2(tmpE.y, tmpE.x) * 180 / M_PI);
        currentBeeBox->phi = static_cast<float>(fmod(currentBeeBox->phi + phiTemp, 360));
        m_last_rotation_point = cv::Point2i(static_cast<int>(e->x()), static_cast<int>(e->y()));
        m_path_changed = true;
    }
    Q_EMIT update();
}

void BeeDanceTracker::mouseReleaseEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (getVideoMode() != GuiParam::VideoMode::Paused) return;

    if (e->button() == Qt::LeftButton && e->modifiers() != Qt::ControlModifier) {
        if (m_rectstat >= RS_SET) {
            m_rectstat = RS_SET;
            Q_EMIT update();
        }
    } else if( e->button() == Qt::LeftButton) {
        if (m_rectstat >= RS_SET) {
            m_rectstat = RS_SET;
            Q_EMIT update();
        }
    } else if (e->button() == Qt::RightButton) {
        if (m_rectstat == RS_ROTATE) {
            m_rectstat = RS_SET;
            Q_EMIT update();
        }
    }
}

void BeeDanceTracker::mouseWheelEvent ( QWheelEvent *) { }

/*
* draws every path currently in the paths vector
*/
void BeeDanceTracker::drawPath(QPainter *painter){
    if (m_cto >= static_cast<int>(m_trackedObjects.size()) || !m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) return;

    auto o = m_trackedObjects[m_cto];
    for (size_t frame = 1; frame < o.getLastFrameNumber().get() + 1; frame++) {
        if (o.hasValuesAtFrame(frame) && o.hasValuesAtFrame(frame-1)) {
            BeeBox point1 = o.get<BeeBox>(frame - 1);
            BeeBox point2 = o.get<BeeBox>(frame);

            QPoint p1 = QPoint(static_cast<int>(point1.x), static_cast<int>(point1.y));
            QPoint p2 = QPoint(static_cast<int>(point2.x), static_cast<int>(point2.y));
            if (frame > m_currentFrame) {
                painter->setPen(QColor(FUTURE_TRACK_COLOR));
                painter->drawLine(p1, p2);
            } else {
                painter->setPen(QColor(PAST_TRACK_COLOR));
                painter->drawLine(p1, p2);
            }
        }
    }
}

/*
* this will draw the tracked area's bounding box and its handles onto the display image
*/
void BeeDanceTracker::drawRectangle(QPainter *painter, int frame) {
    for (auto o : m_trackedObjects) {
        int tmpFrame;
        QColor c;
        if (o.hasValuesAtFrame(static_cast<int>(frame))) {
            tmpFrame = frame;
            c = static_cast<int>(o.getId())==m_cto?QColor(BOX_COLOR):QColor(BOX_COLOR_INACTIVE);
        } else {
            if(o.getLastFrameNumber()) {
                tmpFrame = o.getLastFrameNumber().get();
                c = static_cast<int>(o.getId()) == m_cto ? QColor(BOX_COLOR, 60) : QColor(BOX_COLOR_INACTIVE, 60);
            } else {
                break;
            }
        }
        QPen pen = QPen(c);
        pen.setWidthF(1.5);
        painter->setPen(pen);

        auto currentBeeBox = o.get<BeeBox>(tmpFrame);

        std::vector<cv::Point2i> box = currentBeeBox->getCornerPoints();

        //draw the bounding box
        for (int i = 0; i < 4; i++) {
            painter->drawLine(box[i].x, box[i].y, box[(i + 1) % 4].x, box[(i + 1) % 4].y);
        }

        std::vector<QPointF> arrow = getArrowPoints(tmpFrame, o.getId());
        // direction indicator
        painter->drawLine(arrow[0], arrow[1]);
        painter->drawLine(arrow[0], arrow[2]);
        painter->drawLine(arrow[0], arrow[3]);

        // draw id
        QPointF textCenter = QPointF(currentBeeBox->x + sin(currentBeeBox->phi* M_PI / 180) * currentBeeBox->h * -0.4,
                                     currentBeeBox->y + cos(currentBeeBox->phi* M_PI / 180) * currentBeeBox->h * -0.4);
        int textheight = currentBeeBox->h*0.15>16.0?16:static_cast<int>(currentBeeBox->h * 0.15);
        textheight = textheight>0?textheight:1;

        QFont font = painter->font();
        font.setPointSize(textheight);
        painter->setFont(font);

        painter->translate(textCenter);
        painter->rotate(-currentBeeBox->phi);

        if(currentBeeBox->phi > 90 || currentBeeBox->phi < -90 || currentBeeBox->phi > 270){
            painter->rotate(180);
        }

        painter->drawText(QRectF(-currentBeeBox->w/2,-currentBeeBox->h*0.15/2,currentBeeBox->w,currentBeeBox->h*0.15),
                          Qt::AlignCenter, std::to_string(o.getId()).c_str());

        if(currentBeeBox->phi > 90 || currentBeeBox->phi < -90 || currentBeeBox->phi > 270){
            painter->rotate(-180);
        }

        painter->rotate(currentBeeBox->phi);
        painter->translate(-textCenter);

        // draw resize points on active box
        if (static_cast<int>(o.getId()) == m_cto && tmpFrame == frame) {
            //make sure the corner points are up to date
            updatePoints(tmpFrame);

            QPen dotPen = QPen(QColor(0,0,0));
            pen.setWidth(2);
            QBrush dotBrush = QBrush(QColor(230,230,255));

            int srad = 6;
            //disable transformation during playback
            if (getVideoMode() != GuiParam::VideoMode::Playing) {
                //scaling handles
                for (int i = 0; i < 4; i++) {
                    painter->setPen(dotPen);
                    painter->setBrush(dotBrush);
                    painter->drawEllipse(QPoint(m_pts[i].x, m_pts[i].y), srad, srad);
                }
            }
        }
    }
}

/*
* update the corner points by calculating their current positions based on the centre, width, height and rotation of the mask
*/
void BeeDanceTracker::updatePoints(int frame) {
    if(!m_trackedObjects[m_cto].hasValuesAtFrame(frame)) {
        // this happens after the video gets paused and getCurrentFrameNumber()
        // returns currently painted frame + 1
        return;
    }
    auto currentBeeBox = m_trackedObjects[m_cto].get<BeeBox>(frame);

    m_pts = currentBeeBox->getCornerPoints();
}

/*
 * calculates the points needed to draw the arrow inside the box
 */
std::vector<QPointF> BeeDanceTracker::getArrowPoints(int frame, int cto) {
    if(!m_trackedObjects[cto].hasValuesAtFrame(frame)) {
        return std::vector<QPointF>(4);
    }
    auto currentBeeBox = m_trackedObjects[cto].get<BeeBox>(frame);

    double h = currentBeeBox->h / 2;
    double w = currentBeeBox->w / 2;
    double p = currentBeeBox->phi * M_PI / 180;

    std::vector<QPointF> arrow(4);
    arrow[0] = QPointF(currentBeeBox->x + sin(p) * h * 0.6, currentBeeBox->y + cos(p) * h * 0.6);
    arrow[1] = QPointF(currentBeeBox->x + sin(p) * h * -0.6, currentBeeBox->y + cos(p) * h * -0.6);
    arrow[2] = QPointF(currentBeeBox->x + sin(p) * h * 0.6 + sin(p - (135*M_PI/180)) * w * 0.6,
                       currentBeeBox->y + cos(p) * h * 0.6 + cos(p - (135*M_PI/180)) * w * 0.6);
    arrow[3] = QPointF(currentBeeBox->x + sin(p) * h * 0.6 + sin(p + (135*M_PI/180)) * w * 0.6,
                       currentBeeBox->y + cos(p) * h * 0.6 + cos(p + (135*M_PI/180)) * w * 0.6);
    return arrow;
}

// =========== P R I V A T E = F U N C S ============


// ============== GUI HANDLING ==================



/*
* enables/disables fixed ratio of the bounding box
*/
void BeeDanceTracker::fixRatio() {
    if(m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) {
        auto currentBeeBox = m_trackedObjects[m_cto].get<BeeBox>(m_currentFrame);
        if (!m_fixedratio && currentBeeBox->w != 0) {
            m_ratio = currentBeeBox->h / currentBeeBox->w;
        }
    }
    m_fixedratio = !m_fixedratio;
    m_fixedratioEdit->setChecked(m_fixedratio);
    Q_EMIT update();
}

/*
* enables/disables drawing of the paths
*/
void BeeDanceTracker::showPath() {
    m_path_showing = !m_path_showing;
    Q_EMIT update();
}

/*
* enables/disables correction of the path
*/
void BeeDanceTracker::enableCorrection() {
    m_correction_enabled = !m_correction_enabled;
    m_noncorrectionstepsEdit->setDisabled(!m_correction_enabled);
    Q_EMIT update();
}

/*
* switches between automatic and semi-automatic tracking
*/
void BeeDanceTracker::switchMode() {
    m_automatictracking = !m_automatictracking;
    if (m_automatictracking) {
        m_of_tracker = new OverlapOFTracker();
        m_noncorrectionstepsEdit->setDisabled(!m_correction_enabled);
        m_enable_correction->setDisabled(!m_automatictracking);
    } else {
        m_of_tracker = new SingleOFTracker();
        m_noncorrectionstepsEdit->setDisabled(!m_automatictracking);
        m_enable_correction->setDisabled(!m_automatictracking);
    }
    m_futurestepsEdit->setDisabled(!m_automatictracking);
    Q_EMIT update();
}

/*
* reads new parameter values and changes the trackers to use them
*/
void BeeDanceTracker::changeParams() {
    int temp1 = m_futurestepsEdit->text().toInt();
    int temp2 = m_featuresEdit->text().toInt();
    m_noncorrectionsteps = m_noncorrectionstepsEdit->text().toInt();
    if (m_automatictracking) {
        if (temp1 != m_futuresteps || temp2 != m_features) {
            m_of_tracker = new OverlapOFTracker();
        }
        m_futuresteps = temp1;
        m_features = temp2;
        reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->configure(m_futuresteps, m_noncorrectionsteps, m_features, m_correction_enabled);
    } else {
        m_features = temp2;
        reinterpret_cast<SingleOFTracker*>(m_of_tracker)->configure(m_features);
    }
}