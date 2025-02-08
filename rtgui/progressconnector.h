/*
 *  This file is part of RawTherapee.
 *
 *  Copyright (c) 2004-2010 Gabor Horvath <hgabor@rawtherapee.com>
 *
 *  RawTherapee is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  RawTherapee is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RawTherapee.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <gtkmm.h>

#include <sigc++/sigc++.h>

#include "guiutils.h"
#include "multilangmgr.h"
#include "rtengine/rtengine.h"

#include <thread>

class PLDBridge final :
    public rtengine::ProgressListener
{
public:
    explicit PLDBridge(rtengine::ProgressListener* pb) :
        pl(pb)
    {
    }

    // ProgressListener interface
    void setProgress(double p) override
    {
        GuiThreadSafety::assertInGuiThread();
        pl->setProgress(p);
    }
    void setProgressStr(const Glib::ustring& str) override
    {
        GuiThreadSafety::assertInGuiThread();
        Glib::ustring progrstr;
        progrstr = M(str);
        pl->setProgressStr(progrstr);
    }

    void setProgressState(bool inProcessing) override
    {
        GuiThreadSafety::assertInGuiThread();
        pl->setProgressState(inProcessing);
    }

    void error(const Glib::ustring& descr) override
    {
        GuiThreadSafety::assertInGuiThread();
        pl->error(descr);
    }

private:
    rtengine::ProgressListener* const pl;
};

template<class T>
class ProgressConnector
{

    sigc::signal<T()> opStart;
    sigc::signal<bool()> opEnd;
    T retval;
    std::thread workThread;

    static int emitEndSignalUI (void* data)
    {
        const auto lopEnd = reinterpret_cast<sigc::signal<bool()>*>(data);
        const int r = lopEnd->emit ();
        delete lopEnd;

        return r;
    }

    void workingThread ()
    {
        retval = opStart.emit ();
        g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, ProgressConnector<T>::emitEndSignalUI,
                        new sigc::signal<bool()>(opEnd), nullptr);
    }

public:

    ProgressConnector () : retval( 0 ) {}
    ~ProgressConnector() {
        if (workThread.joinable()) {
            workThread.join();
        }
    }

    void startFunc (const sigc::slot<T()>& startHandler, const sigc::slot<bool()>& endHandler )
    {
        if (!workThread.joinable()) {
            opStart.connect (startHandler);
            opEnd.connect (endHandler);
            workThread = std::thread(&ProgressConnector<T>::workingThread, this);
        }
    }

    T returnValue() const { return retval; }
};
