/*
 * This file is part of Krita
 *
 * Copyright (c) 2005 Casper Boemann <cbr@boemann.dk>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include <qlayout.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qlabel.h>
#include <qcombobox.h>

#include "kis_filter_config_widget.h"
#include "kis_perchannel_filter.h"
#include "wdg_perchannel.h"
#include "kis_colorspace.h"
#include "kis_paint_device_impl.h"
#include "kis_iterators_pixel.h"
#include "kcurve.h"
#include "kis_histogram.h"
#include "kis_basic_histogram_producers.h"


KisPerChannelFilterConfiguration::KisPerChannelFilterConfiguration(int n)
{
    for(int i=0;i<n;i++)
         transfers[i] = new Q_UINT16[256];
    m_nTransfers=n;
}

KisPerChannelFilterConfiguration::~KisPerChannelFilterConfiguration()
{
    for(int i=0;i<m_nTransfers;i++)
        delete [] transfers[i];
}

KisPerChannelFilter::KisPerChannelFilter()
    : KisFilter( id(), "adjust", "&Color Adjustment...")
{

}

KisFilterConfigWidget * KisPerChannelFilter::createConfigurationWidget(QWidget *parent, KisPaintDeviceImplSP dev)
{
    return new KisPerChannelConfigWidget(parent, dev);
}

KisFilterConfiguration* KisPerChannelFilter::configuration(QWidget *nwidget, KisPaintDeviceImplSP)
{
    KisPerChannelConfigWidget* widget = (KisPerChannelConfigWidget*)nwidget;

    if ( widget == 0 )
    {
        return new KisPerChannelFilterConfiguration(3);
    } else {
        return widget->config();
    }
}

std::list<KisFilterConfiguration*> KisPerChannelFilter::listOfExamplesConfiguration(KisPaintDeviceImplSP /*dev*/)
{
//XXX should really come up with a list of configurations
    std::list<KisFilterConfiguration*> list;
//    list.insert(list.begin(), new KisPerChannelFilterConfiguration(m_dev->colorspace()->nColorChannels()));
    return list;
}


void KisPerChannelFilter::process(KisPaintDeviceImplSP src, KisPaintDeviceImplSP dst, KisFilterConfiguration* config, const QRect& rect)
{
    KisPerChannelFilterConfiguration* configBC = (KisPerChannelFilterConfiguration*) config;

    KisColorAdjustment *adj = src->colorSpace()->createPerChannelAdjustment(configBC->transfers);

    KisRectIteratorPixel iter = dst->createRectIterator(rect.x(), rect.y(), rect.width(), rect.height(), true );

    setProgressTotalSteps(rect.width() * rect.height());
    Q_INT32 pixelsProcessed = 0;

    while( ! iter.isDone()  && !cancelRequested())
    {
        Q_UINT32 npix=0, maxpix = iter.nConseqPixels();
        Q_UINT8 selectedness = iter.selectedness();
        // The idea here is to handle stretches of completely selected and completely unselected pixels.
        // Partially selected pixels are handled one pixel at a time.
        switch(selectedness)
        {
            case MIN_SELECTED:
                while(iter.selectedness()==MIN_SELECTED && maxpix)
                {
                    --maxpix;
                    ++iter;
                    ++npix;
                }
                pixelsProcessed += npix;
                break;

            case MAX_SELECTED:
            {
                Q_UINT8 *firstPixel = iter.rawData();
                while(iter.selectedness()==MAX_SELECTED && maxpix)
                {
                    --maxpix;
                    ++iter;
                    ++npix;
                }
                // adjust
                src->colorSpace()->applyAdjustment(firstPixel, firstPixel, adj, npix);
                pixelsProcessed += npix;
                break;
            }

            default:
                // adjust, but since it's partially selected we also only partially adjust
                src->colorSpace()->applyAdjustment(iter.oldRawData(), iter.rawData(), adj, 1);
                const Q_UINT8 *pixels[2] = {iter.oldRawData(), iter.rawData()};
                Q_UINT8 weights[2] = {MAX_SELECTED - selectedness, selectedness};
                src->colorSpace()->mixColors(pixels, weights, 2, iter.rawData());
                ++iter;
                pixelsProcessed++;
                break;
        }
        setProgress(pixelsProcessed);
    }
    setProgressDone();
}

void KisPerChannelConfigWidget::setActiveChannel(int ch)
{
    int i;
    int height = 256;
    QPixmap pix(256, height);
    pix.fill();
    QPainter p(&pix);
    p.setPen(QPen::QPen(Qt::gray,1, Qt::SolidLine));

    m_histogram->setChannel(ch);

    double highest = (double)m_histogram->calculations().getHighest();
    Q_INT32 bins = m_histogram->producer() -> numberOfBins();

    if (m_histogram->getHistogramType() == LINEAR) {
        double factor = (double)height / highest;
        for( i=0; i<bins; ++i ) {
            p.drawLine(i, height, i, height - int(m_histogram->getValue(i) * factor));
        }
    } else {
        double factor = (double)height / (double)log(highest);
        for( i = 0; i < bins; ++i ) {
            p.drawLine(i, height, i, height - int(log((double)m_histogram->getValue(i)) * factor));
        }
    }

    m_curves[m_activeCh].setAutoDelete(true);
    m_curves[m_activeCh] = m_page->kCurve->getCurve();
    m_activeCh = ch;
    m_page->kCurve->setCurve(m_curves[m_activeCh]);

    m_page->kCurve->setPixmap(pix);
}

KisPerChannelConfigWidget::KisPerChannelConfigWidget(QWidget * parent, KisPaintDeviceImplSP dev, const char * name, WFlags f)
    : KisFilterConfigWidget(parent, name, f)
{
    int i;
    int height;
    m_page = new WdgPerChannel(this);
    QHBoxLayout * l = new QHBoxLayout(this);
    Q_CHECK_PTR(l);

    m_dev = dev;
    m_curves = new QSortedList<QPair<double,double> >[m_dev->colorSpace()->nColorChannels()];
    m_activeCh = 0;
    for(int ch=0; ch <m_dev->colorSpace()->nColorChannels(); ch++)
    {
        m_curves[ch].append(new QPair<double,double>(0, 0));
        m_curves[ch].append(new QPair<double,double>(1, 1));
    }

    l -> add(m_page);
    height = 256;
    connect( m_page->kCurve, SIGNAL(modified()), SIGNAL(sigPleaseUpdatePreview()));

    // Fill in the channel chooser
    QValueVector<KisChannelInfo *> channels = dev->colorSpace()->channels();
    for(i=0; i < dev->colorSpace()->nColorChannels(); i++)
        m_page->cmbChannel -> insertItem(channels.at(i)->name());
    connect( m_page->cmbChannel, SIGNAL(activated(int)), this, SLOT(setActiveChannel(int)));

    // Create the horizontal gradient label
    QPixmap hgradientpix(256, 1);
    QPainter hgp(&hgradientpix);
    hgp.setPen(QPen::QPen(QColor(0,0,0),1, Qt::SolidLine));
    for( i=0; i<256; ++i )
    {
        hgp.setPen(QColor(i,i,i));
        hgp.drawPoint(i, 0);
    }
    m_page->hgradient->setPixmap(hgradientpix);

    // Create the vertical gradient label
    QPixmap vgradientpix(1, 256);
    QPainter vgp(&vgradientpix);
    vgp.setPen(QPen::QPen(QColor(0,0,0),1, Qt::SolidLine));
    for( i=0; i<256; ++i )
    {
        vgp.setPen(QColor(i,i,i));
        vgp.drawPoint(0, 255-i);
    }
    m_page->vgradient->setPixmap(vgradientpix);

    KisIDList keys = 
        KisHistogramProducerFactoryRegistry::instance()->listKeysCompatibleWith(m_dev->colorSpace());
    KisHistogramProducerFactory *hpf;
    // workaround until we have 'default' producer for a cs
    if ((*keys.at(0)) == KisID("GENLIGHTHISTO", ""))
        hpf = KisHistogramProducerFactoryRegistry::instance()->get(*(keys.at(1)));
    else
        hpf = KisHistogramProducerFactoryRegistry::instance()->get(*(keys.at(0)));
    m_histogram = new KisHistogram(m_dev, hpf->generate(), LINEAR);

    setActiveChannel(0);
}

KisPerChannelFilterConfiguration * KisPerChannelConfigWidget::config()
{
    int nCh = m_dev->colorSpace()->nColorChannels();
    KisPerChannelFilterConfiguration * cfg = new KisPerChannelFilterConfiguration(nCh);

    m_curves[m_activeCh].setAutoDelete(true);
    m_curves[m_activeCh] = m_page->kCurve->getCurve();
    for(int ch = 0; ch < nCh; ch++)
    {
        for(int i=0; i <256; i++)
        {
            Q_INT32 val;
            val = int(0xFFFF * m_page->kCurve->getCurveValue( m_curves[ch],  i / 255.0));
            if(val >0xFFFF)
                val=0xFFFF;
            if(val <0)
                val = 0;

            cfg->transfers[ch][i] = val;
        }
    }
    return cfg;
}

#include "kis_perchannel_filter.moc"
