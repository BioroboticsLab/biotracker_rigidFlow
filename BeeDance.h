#pragma once

#include "OverlapOFTracker.h"
#include "SingleOFTracker.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>

#include <QPointer>
#include <QSlider>
#include <QLabel>

#include <biotracker/TrackingAlgorithm.h>
#include <biotracker/util/MutexWrapper.h>

#include <opencv2/video/tracking.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <ctype.h>

class BeeDanceTracker : public BioTracker::Core::TrackingAlgorithm {
    Q_OBJECT
  public:
    BeeDanceTracker(BioTracker::Core::Settings &settings);

    void track(size_t frameNumber, const cv::Mat &frame) override;
    void paint(size_t frameNumber, BioTracker::Core::ProxyMat &m, View const &view = OriginalView) override;
    void paintOverlay(size_t frameNumber, QPainter *painter, View const &view = OriginalView) override;

    std::set<Qt::Key> const &grabbedKeys() const override {
        return m_grabbedKeys;
    }

    void prepareSave() override;

    void postLoad() override;

    void keyPressEvent(QKeyEvent *ev) override;

    //mouse click and move events
    void mouseMoveEvent(QMouseEvent *e);
    void mousePressEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);
    void mouseWheelEvent(QWheelEvent *e);

  private:
    size_t				        m_currentFrame; // is always the current frame (updated in paint and track)
    cv::Mat                     m_currentImage;
    bool                        m_tmpBeeBox;
    bool                        m_diff_path;
    // --
    int                         m_futuresteps;
    int                         m_noncorrectionsteps;
    bool                        m_correction_enabled;
    int                         m_features;
    bool                        m_automatictracking;
    bool                        m_updatefeatures;

    bool                        m_fixedratio;
    bool                        m_path_showing;
    bool                        m_path_changed;

    float                       m_ratio;

    std::vector<cv::Point2i>    m_pts;

    cv::Point2i                 m_last_rotation_point;

    int                         m_mdx;
    int                         m_mdy;

    int                         m_rectstat;
    double                      m_rotation;

    OFTracker*                  m_of_tracker;
    int                      m_cto;

    std::set<Qt::Key>	        m_grabbedKeys;

    // as we want to adapt the values of this class all the time we need to
    // keep it accessible from other methods in the object..
    QLineEdit	*			m_futurestepsEdit;
    QLineEdit	*			m_noncorrectionstepsEdit;
    QCheckBox   *			m_enable_correction;
    QLineEdit   *           m_featuresEdit;
    QCheckBox   *           m_fixedratioEdit;

  private Q_SLOTS:
    void switchToATracking();
    void switchToSATracking();
    void fixRatio();
    void changeParams();
    void enableCorrection();
    void showPath();
    void deletePath();

    void switchMode(bool atracking);
    bool clickInsideRectangle(std::vector<cv::Point2i> pts, QMouseEvent *e);
    void drawPath(QPainter *painter);
    void drawRectangle(QPainter *painter, int frame);
    void updatePoints(int frame);
    std::vector<QPointF> getArrowPoints(int frame, int cto);
};
