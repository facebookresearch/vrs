/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "FrameWidget.h"

#include <sstream>

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qimage.h>
#include <qmenu.h>
#include <qpainter.h>
#include <qsizepolicy.h>

#define DEFAULT_LOG_CHANNEL "FrameWidget"
#include <logging/Log.h>
#include <vrs/TagConventions.h>
#include <vrs/utils/PixelFrame.h>

using namespace vrs;
using namespace std;

namespace {
bool convertToQImageFormat(const vrs::PixelFormat format, QImage::Format& formatOut) {
  switch (format) {
    case vrs::PixelFormat::GREY8:
      formatOut = QImage::Format_Grayscale8;
      break;
    case vrs::PixelFormat::RGB8:
      formatOut = QImage::Format_RGB888;
      break;
    case vrs::PixelFormat::RGBA8:
      formatOut = QImage::Format_RGBA8888;
      break;
    default:
      formatOut = QImage::Format_Grayscale8;
      return false;
  }
  return true;
}
} // namespace

namespace vrsp {

FrameWidget::FrameWidget(QWidget* parent) {
  setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  setContextMenuPolicy(Qt::CustomContextMenu);
  connect(this, &FrameWidget::customContextMenuRequested, this, &FrameWidget::ShowContextMenu);
}

void FrameWidget::paintEvent(QPaintEvent* evt) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRect wrect = painter.window();
  const QRect rect = painter.viewport();
  int hOffset = 0;
  int vOffset = 0;
  painter.setPen(overlayColor_);
  bool hasImage = false;
  { // mutex scoope
    unique_lock lock(mutex_);
    PixelFrame* image = image_.get();
    if (image != nullptr) {
      QImage::Format qformat;
      hasImage = convertToQImageFormat(image->getPixelFormat(), qformat);
      if (hasImage) {
        QSize size = image->qsize();
        QImage qImage(
            image->rdata(),
            size.width(),
            size.height(),
            static_cast<int>(image->getStride()),
            qformat);
        painter.translate(rect.width() / 2, rect.height() / 2);
        QSize rsize = size;
        rsize.scale(rotate(rect.size()), Qt::KeepAspectRatio);
        bool sideWays = rotation_ % 180 != 0;
        painter.scale(
            ((flipped_ && !sideWays) ? -1. : 1.) * rsize.width() / size.width(),
            ((flipped_ && sideWays) ? -1. : 1.) * rsize.height() / size.height());
        painter.rotate(rotation_);
        painter.drawImage(-size.width() / 2, -size.height() / 2, qImage);
        rsize = rotate(rsize);
        hOffset = (rect.width() - rsize.width()) / 2;
        vOffset = (rect.height() - rsize.height()) / 2;

        // undo scaling
        painter.setViewport(rect);
        painter.setWindow(wrect);
        painter.resetTransform();
      } else {
        XR_LOGW(
            "Could not convert pixel format {} to a Qt equivalent. "
            "Falling back to Grayscale8, but you'll probably see nothing.",
            vrs::toString(image->getPixelFormat()));

        if (painter.font().pointSize() != 16) {
          QFont font = painter.font();
          font.setPointSize(16);
          painter.setFont(font);
        }
        painter.setPen(Qt::black);
        QString description =
            QString::fromStdString(vrs::toString(image->getPixelFormat()).c_str()) +
            " pixel format not supported...";
        painter.drawText(QPointF(rect.left() + 4, rect.bottom() - 4), description);
      }
    }
  } // mutex scoope

  // draw description overlay
  if (painter.font().pointSize() != 14) {
    QFont font = painter.font();
    font.setPointSize(14);
    painter.setFont(font);
  }
  QString description = descriptions_.getDescription(typeToShow_);
  painter.drawText(rect.adjusted(hOffset, vOffset + 4, 2, 2), Qt::AlignLeft, description);

  // draw fps overlay
  if (hasImage && dataFps_ > 0) {
    QString fpsStr = QString("%1/%2").arg(dataFps_).arg(imageFps_.value());
    int fps = drawFps_.newFrame();
    if (fps > 0) {
      fpsStr += QString("-%1").arg(fps);
    }
    fpsStr += " fps";
    QRect r(rect.left() + hOffset + 4, rect.top(), rect.width(), rect.height() - vOffset - 4);
    painter.drawText(r, Qt::AlignLeft | Qt::AlignBottom, fpsStr);
  }
}

QSize FrameWidget::sizeHint() const {
  return getImageSize().scaled(QSize(500, 500), Qt::KeepAspectRatio);
}

int FrameWidget::heightForWidth(int w) const {
  QSize size = getImageSize();
  return (w * size.height()) / size.width();
}

void FrameWidget::swapImage(shared_ptr<PixelFrame>& image) {
  unique_lock lock(mutex_);
  imageFps_.newFrame();
  bool resize = image && image->qsize() != imageSize_;
  image_.swap(image);
  if (resize) {
    imageSize_ = image_->qsize();
    updateMinMaxSize();
  }
  setNeedsUpdate();
}

void FrameWidget::updateMinMaxSize() {
  if (image_) {
    QSize size = getImageSize();
    setMinimumSize(size.scaled(100, 100, Qt::KeepAspectRatio));
    setBaseSize(size);
    QSize screen = QApplication::desktop()->screenGeometry(this).size().operator*=(0.95);
    setMaximumSize(size.scaled(screen, Qt::KeepAspectRatio));
  }
}

void FrameWidget::ShowContextMenu(const QPoint& pos) {
  QMenu contextMenu(tr("Context menu"), this);
  QAction noRotation("No Rotation", this);
  connect(&noRotation, &QAction::triggered, [this]() { this->setRotation(0); });
  noRotation.setCheckable(true);
  noRotation.setChecked(rotation_ == 0);
  contextMenu.addAction(&noRotation);
  QAction rotate90("Rotate Right", this);
  connect(&rotate90, &QAction::triggered, [this]() { this->setRotation(90); });
  rotate90.setCheckable(true);
  rotate90.setChecked(rotation_ == 90);
  contextMenu.addAction(&rotate90);
  QAction rotate180("Rotate Upside-Down", this);
  connect(&rotate180, &QAction::triggered, [this]() { this->setRotation(180); });
  rotate180.setCheckable(true);
  rotate180.setChecked(rotation_ == 180);
  contextMenu.addAction(&rotate180);
  QAction rotate270("Rotate Left", this);
  connect(&rotate270, &QAction::triggered, [this]() { this->setRotation(270); });
  rotate270.setCheckable(true);
  rotate270.setChecked(rotation_ == 270);
  contextMenu.addAction(&rotate270);

  contextMenu.addSeparator();
  QAction mirror("Mirror Image", this);
  connect(&mirror, &QAction::triggered, [this]() { this->setFlipped(!flipped_); });
  mirror.setCheckable(true);
  mirror.setChecked(flipped_);
  contextMenu.addAction(&mirror);

  contextMenu.addSeparator();
  QAction before("Move Before", this);
  connect(&before, &QAction::triggered, [this]() { emit this->shouldMoveBefore(); });
  contextMenu.addAction(&before);
  QAction after("Move After", this);
  connect(&after, &QAction::triggered, [this]() { emit this->shouldMoveAfter(); });
  contextMenu.addAction(&after);
  QAction hide("Hide Stream", this);
  connect(&hide, &QAction::triggered, [this]() { emit this->shouldHideStream(); });
  contextMenu.addAction(&hide);

  contextMenu.exec(mapToGlobal(pos));
}

void FrameWidget::blank() {
  {
    unique_lock lock(mutex_);
    if (image_) {
      image_->blankFrame();
    }
  }
  descriptions_.clearDescription();
  dataFps_ = 0;
  imageFps_.reset();
  drawFps_.reset();
  setNeedsUpdate();
}

void FrameWidget::setDeviceName(const string& deviceName) {
  vector<string> tooltip;
  if (!deviceName.empty()) {
    tooltip.emplace_back(deviceName);
  }
  if (!deviceTypeTag_.empty() &&
      (deviceTypeConfig_.empty() || deviceTypeConfig_ != deviceTypeTag_)) {
    const char* label = !deviceTypeConfig_.empty() ? "Device type tag: " : "Device type: ";
    tooltip.emplace_back(label + deviceTypeTag_);
  }
  if (!deviceTypeConfig_.empty()) {
    tooltip.emplace_back("Device type: " + deviceTypeConfig_);
  }
  auto tooltipStr = fmt::format("{}", fmt::join(tooltip, "\n"));
  setToolTip(QString::fromUtf8(tooltipStr.c_str()));
}

void FrameWidget::setDeviceType(const string& deviceType) {
  deviceTypeConfig_ = deviceType;
}

void FrameWidget::setDescription(
    Record::Type recordType,
    size_t blockIndex,
    const string& description) {
  descriptions_.setDescription(recordType, blockIndex, description);
}

void FrameWidget::setDescriptions(
    Record::Type recordType,
    const map<size_t, QString>& descriptions) {
  descriptions_.setDescriptions(recordType, descriptions);
}

void FrameWidget::setTags(const map<string, string>& tags) {
  stringstream ss;
  for (const auto& tag : tags) {
    if (tag.first == vrs::tag_conventions::kDeviceType) {
      deviceTypeTag_ = tag.second;
    }
    ss << "  " << tag.first << ": " << tag.second << "\n";
  }
  descriptions_.setDescription(Record::Type::TAGS, 0, ss.str());
}

void FrameWidget::resetOrientation() {
  rotation_ = 0;
  flipped_ = false;
  setNeedsUpdate();
  emit orientationChanged();
}

void FrameWidget::setRotation(int rotation) {
  rotation_ = rotation;
  updateMinMaxSize();
  setNeedsUpdate();
  emit orientationChanged();
}

void FrameWidget::setFlipped(bool flipped) {
  flipped_ = flipped;
  setNeedsUpdate();
}

QSize FrameWidget::rotate(QSize size) const {
  return (rotation_ % 180) == 0 ? size : size.transposed();
}

QSize FrameWidget::getImageSize() const {
  return rotate(imageSize_);
}

} // namespace vrsp
