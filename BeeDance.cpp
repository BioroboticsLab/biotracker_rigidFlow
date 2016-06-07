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

#define BOX_COLOUR 255, 30, 20
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
    m_show_features(true),
    m_ratio(2.5),
    m_rectstat(RS_NOT_SET),
    m_of_tracker(new OverlapOFTracker()),
    m_cto(0),
    m_changedPath(false),
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

    auto *showFeatures = new QCheckBox();
    showFeatures->setChecked(m_show_features);
    QObject::connect(showFeatures, &QCheckBox::stateChanged,
                     this, &BeeDanceTracker::showFeatures);
    layout->addRow("Show Features", showFeatures);

    ui->setLayout(layout);
}

void BeeDanceTracker::track(size_t frame, const cv::Mat &imgOriginal) {
    m_currentFrame = frame; // TODO must this be protected from other threads?

    cv::Mat imgCopy = imgOriginal.clone();

    if(imgCopy.empty()) return;

    // initialize new Path
    if (m_cto >= static_cast<int>(m_trackedObjects.size())) {
        const size_t id = m_trackedObjects.size(); // position in list + id are correlated
        auto bb = std::make_shared<BeeBox>();
        TrackedObject o(id);
        o.add(frame, bb);
        m_trackedObjects.push_back(o);
    }
    // skipped through video
    if(!m_trackedObjects[m_cto].hasValuesAtFrame(frame) && !m_trackedObjects[m_cto].hasValuesAtFrame(frame - 1)){
        return;
    }
    //the video is playing or paused but the previous/next buttons were used
    if((getVideoMode() == GuiParam::VideoMode::Playing || abs(getCurrentFrameNumber() - static_cast<long>(frame)) == 0) && isTrackingActivated()) {
        if (m_start_of_tracking) {
            m_start_of_tracking = false;
        }
        if (!m_of_tracker->isInitialized()) {
            //semi-automatic or automatic tracking
            if (!m_automatictracking) {
                reinterpret_cast<SingleOFTracker*>(m_of_tracker)->configure(m_features, m_show_features);
                reinterpret_cast<SingleOFTracker*>(m_of_tracker)->init(imgCopy);
            }
            else {
                reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->configure(m_futuresteps, m_noncorrectionsteps, m_features, m_correction_enabled, m_show_features);
                reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->init(imgCopy);
            }
        }
        //copy BeeBox from previous frame
        if (!m_trackedObjects[m_cto].hasValuesAtFrame(frame)) {
            auto o = std::make_shared<BeeBox>(m_trackedObjects[m_cto].get<BeeBox>(frame - 1));
            m_trackedObjects[m_cto].push_back(o);
        }
        //calculate movement for next step
        m_of_tracker->next(imgCopy, *m_trackedObjects[m_cto].get<BeeBox>(frame));
    }
    //the video is paused or playing in playback mode
//    else if (getVideoMode() == GuiParam::VideoMode::Paused || (getVideoMode() == GuiParam::VideoMode::Playing && !isTrackingActivated())) {
    else {
        for (uint i = 0; i < m_trackedObjects.size(); i++) {
            if (m_trackedObjects[i].hasValuesAtFrame(frame)) {
                m_cto = i;
            } else {
//                if (static_cast<int>(frame) < m_trackedObjects[0].begin()) {
//                    m_cto = 0;
//                } else {
//                    m_cto = m_trackedObjects.size() - 1;
//                }
            }
            if (isTrackingActivated()) {
                m_of_tracker->reset();
            }
        }
    }
}

void BeeDanceTracker::paint(size_t frame, ProxyMat & mat, const TrackingAlgorithm::View &) {
    m_currentFrame = frame; // TODO must this be protected from other threads?

    if (m_path_showing) {
        drawPath(mat.getMat());
    }
    if (!m_trackedObjects[m_cto].hasValuesAtFrame(static_cast<int>(frame))) {
        return;
    }

    if (m_rectstat >= RS_SET) {
        drawRectangle(mat.getMat(), static_cast<int>(frame));
    }
}

void BeeDanceTracker::paintOverlay(size_t currentFrame, QPainter *painter, const View &) {

}

// =========== I O = H A N D L I N G ============


// ============== Keyboard ==================

void BeeDanceTracker::keyPressEvent(QKeyEvent *ev) {

}

// ============== Mouse ==================

void BeeDanceTracker::mousePressEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (!(getVideoMode() == GuiParam::VideoMode::Paused || m_start_of_tracking)) return;
//    if (m_start_of_tracking) return;

    //check if clicked on path
    m_mouseOverPath = -1;
    size_t maxTs = 0;
    for (auto o : m_trackedObjects) {
        maxTs = o.getLastFrameNumber().get() > maxTs ? o.getLastFrameNumber().get() : maxTs;
    }

    for (size_t frame = 0; frame < maxTs + 1; frame++) {
        for (size_t i = 0; i < m_trackedObjects.size(); i++) {
            auto o = m_trackedObjects[i];
            if (o.hasValuesAtFrame(frame)) {
                BeeBox point = o.get<BeeBox>(frame);
                if (abs(e->x() - static_cast<int>(point.x)) <= 2 && abs(e->y() - static_cast<int>(point.y)) <= 2) {
                    m_mouseOverPath = i;
                    break;
                }
            }
        }
    }

    //check if left button is clicked
    if (e->button() == Qt::LeftButton && m_rectstat == RS_NOT_SET) {
        if(!m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) {
            auto bb = std::make_shared<BeeBox>();
            m_trackedObjects[m_cto].add(m_currentFrame, bb);
        }
        auto currentBeeBox = m_trackedObjects[m_cto].get<BeeBox>(m_currentFrame);
        currentBeeBox->x = e->x();
        currentBeeBox->y = e->y();
        m_rectstat = RS_INITIALIZE;
        currentBeeBox->phi = 0;
    }
    if (e->button() == Qt::LeftButton && m_rectstat == RS_SET) {
        int close = 10;
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
        //rotate mode
        if (m_rectstat == RS_SET && abs(e->x() - m_rot2.x) <= close && abs(e->y() - m_rot2.y) <= close) {
            m_rectstat = RS_ROTATE;
        }
    }
    //deletes a previously selected path that is right-clicked
    if (e->button() == Qt::RightButton) {
        if (m_mouseOverPath > -1) {
            if (m_mouseOverPath == m_cto) {
                auto bb = std::make_shared<BeeBox>(m_trackedObjects[m_cto].get<BeeBox>(m_currentFrame));
                TrackedObject o(m_currentFrame);
                o.add(m_currentFrame, bb);
                m_trackedObjects[m_cto] = o;
            }
            else {
                m_trackedObjects.erase(m_trackedObjects.begin() + m_mouseOverPath);
                if (m_cto > m_mouseOverPath) m_cto -= 1;
            }
            m_mouseOverPath = -1;
        }
        Q_EMIT update();
    }
}

void BeeDanceTracker::mouseMoveEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (!(getVideoMode() == GuiParam::VideoMode::Paused || m_start_of_tracking)) return;
//    if (m_start_of_tracking) return;
    if(!m_trackedObjects[m_cto].hasValuesAtFrame(m_currentFrame)) return;
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

        Q_EMIT update();
    }
        //what we do when we are dragging the bounding box
    else if (m_rectstat == RS_DRAG) {
        currentBeeBox->x += e->x() - m_mdx;
        currentBeeBox->y += e->y() - m_mdy;
        m_mdx = e->x();
        m_mdy = e->y();
    }
        //what we do when we are rotating the bounding box
    else if (m_rectstat == RS_ROTATE) {
        currentBeeBox->phi = static_cast<float>(atan2(e->x() - currentBeeBox->x, e->y() - currentBeeBox->y) * 180 / 3.1415);
    }
    Q_EMIT update();
}

void BeeDanceTracker::mouseReleaseEvent(QMouseEvent * e) {
    //forbidding any mouse interaction while the video is playing
//    if (!(getVideoMode() == GuiParam::VideoMode::Paused || m_start_of_tracking)) return;
//    if (m_start_of_tracking) return;

    if (e->button() == Qt::LeftButton) {
        if (m_rectstat >= RS_SET) {
            m_rectstat = RS_SET;
            //a transformation of the bounding box starts a new path
//            m_changedPath = true;
            changePath();
//            m_of_tracker->reset();
            Q_EMIT update();
        }
    }
}

void BeeDanceTracker::mouseWheelEvent ( QWheelEvent *) {}

/*
* draws every path currently in the paths vector
*/
void BeeDanceTracker::drawPath(cv::Mat image){

    int current = m_currentFrame;
    size_t maxTs = 0;
    for (auto o : m_trackedObjects) {
        maxTs = o.getLastFrameNumber().get() > maxTs ? o.getLastFrameNumber().get() : maxTs;
    }

    for (size_t frame = 1; frame < maxTs + 1; frame++) {
        for (size_t i = 0; i < m_trackedObjects.size(); i++) {
            auto o = m_trackedObjects[i];
            if (o.hasValuesAtFrame(frame) && o.hasValuesAtFrame(frame-1)) {
                BeeBox point1 = o.get<BeeBox>(frame - 1);
                BeeBox point2 = o.get<BeeBox>(frame);

                cv::Point2i p1(static_cast<int>(point1.x), static_cast<int>(point1.y));
                cv::Point2i p2(static_cast<int>(point2.x), static_cast<int>(point2.y));
                if (static_cast<int>(frame) > current) {
                    cv::line(image, p1, p2, cv::Scalar(FUTURE_TRACK_COLOR), static_cast<int>(i) == m_mouseOverPath? 2 : 1);
                } else {
                    cv::line(image, p1, p2, cv::Scalar(PAST_TRACK_COLOR), static_cast<int>(i) == m_mouseOverPath ? 2 : 1);
                }
            }
        }
    }
}

/*
* this will draw the tracked area's bounding box and its handles onto the diplay image
*/
void BeeDanceTracker::drawRectangle(cv::Mat image, int frame)
{
    //make sure the corner points are up to date
    updatePoints(frame);

    cv::Scalar colour = cv::Scalar(BOX_COLOUR);
    cv::Scalar dotcolour1 = cv::Scalar(0, 0, 0);
    cv::Scalar dotcolour2 = cv::Scalar(255, 230, 230);

    int srad = 8;

    //draw the bounding box
    for (int i = 0; i < 4; i++) {
        line(image, m_pts[i], m_pts[(i + 1) % 4], colour, 1, CV_AA);
    }

    //disable transformation during playback
    if (getVideoMode() != GuiParam::VideoMode::Playing) {
        //scaling handles
        for (int i = 0; i < 4; i++) {
            cv::circle(image, cv::Point2i(m_pts[i].x, m_pts[i].y), srad, dotcolour1, -2, CV_AA);
            cv::circle(image, cv::Point2i(m_pts[i].x, m_pts[i].y), srad - 2, dotcolour2, -2, CV_AA);
        }
        //handle for rotation
        cv::line(image, m_rot1, m_rot2, colour, 1, CV_AA);
        cv::circle(image, m_rot2, srad, dotcolour1, -2, CV_AA);
        cv::circle(image, m_rot2, srad - 2, dotcolour2, -2, CV_AA);
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
    
    int h = static_cast<int>(currentBeeBox->h / 2);
    float p = static_cast<float>(currentBeeBox->phi * 3.1415 / 180);

    m_pts = currentBeeBox->getCornerPoints();

    m_rot1 = cv::Point2i(static_cast<int>(currentBeeBox->x + sin(p) * h),
                         static_cast<int>(currentBeeBox->y + cos(p) * h));
    m_rot2 = cv::Point2i(static_cast<int>(currentBeeBox->x + sin(p) * (h*1.5 < 15 ? 15 : h*1.5)),
                         static_cast<int>(currentBeeBox->y + cos(p) * (h*1.5 < 15 ? 15 : h*1.5)));
}


/*
* creates a new path by copying the last waypoint from the previous one or reuses the last path if it is empty
*/
void BeeDanceTracker::changePath(){
    for(size_t i = m_trackedObjects[m_cto].getLastFrameNumber().get(); i > m_currentFrame; i--){
        m_trackedObjects[m_cto].erase(i);
    }
//    if (m_changedPath) {
//        BeeBox copy(m_trackedObjects[m_cto][getCurrentFrameNumber()]);
//        auto toSave = BeeBox(copy);
//        m_trackedObjects[m_cto].deleteLast();
//
//        if (!m_trackedObjects[m_cto].isEmpty()) {
//            m_cto = m_cto + 1;
//            m_trackedObjects.push_back(Path(getCurrentFrameNumber()));
//        }
//        m_trackedObjects[m_cto].add(toSave);
//        m_changedPath = false;
//        if (m_automatictracking) m_of_tracker = new OverlapOFTracker();
//        else m_of_tracker = new SingleOFTracker();
//    }
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
* enables/disables drawing of the tracked features
*/
void BeeDanceTracker::showFeatures() {
    m_show_features = !m_show_features;

    if (!m_automatictracking) {
        reinterpret_cast<SingleOFTracker*>(m_of_tracker)->configure(m_features, m_show_features);
    } else {
        reinterpret_cast<OverlapOFTracker*>(m_of_tracker)->configure(m_futuresteps, m_noncorrectionsteps,
                                                                   m_features, m_correction_enabled, m_show_features);
    }
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
    } else {
        m_of_tracker = new SingleOFTracker();
    }
    m_futurestepsEdit->setDisabled(!m_automatictracking);
    m_noncorrectionstepsEdit->setDisabled(!m_automatictracking);
    m_enable_correction->setDisabled(!m_automatictracking);
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