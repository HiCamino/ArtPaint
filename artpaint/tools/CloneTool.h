/*
 * Copyright 2026, Edwin Kamena
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Edwin Kamena <ekamena@protonmail.com>
 *		Based on FreeLineTool by
 *			Heikki Suhonen <heikki.suhonen@gmail.com>
 *			Karsten Heimrich <host.haiku@gmx.de>
 *			Dale Cieslak <dcieslak@yahoo.com>
 *
 */

#ifndef CLONE_TOOL_H
#define CLONE_TOOL_H

#include "DrawingTool.h"


class BBitmap;
class BCheckBox;
class BView;
class CoordinateQueue;
class ImageView;
class ToolScript;


namespace ArtPaint {
	namespace Interface {
		class NumberSliderControl;
	}
}
using ArtPaint::Interface::NumberSliderControl;


class CloneTool : public DrawingTool {
public:
									CloneTool();

			int32					UseToolWithScript(ToolScript*, BBitmap*);
			ToolScript*				UseTool(ImageView*, uint32, BPoint, BPoint);

			status_t				readSettings(BFile&, bool);
			status_t				writeSettings(BFile&);

			BView*					ConfigView();
			const void*				ToolCursor() const;
			const char*				HelpString(bool isInUse) const;
			void					DrawTool(BView* view);

private:
			bool					reading_coordinates;

			ImageView*				image_view;
			CoordinateQueue*		coordinate_queue;
			BPoint					origin;
			BPoint					origin_delta;
			BPoint					point_delta;
};


class CloneToolConfigView : public DrawingToolConfigView {
public:
									CloneToolConfigView(DrawingTool* tool);
	virtual							~CloneToolConfigView() {}

	virtual	void					AttachedToWindow();
	virtual void					MessageReceived(BMessage* message);

private:
			NumberSliderControl*	fLineSize;
			BCheckBox*				fUseBrush;
			NumberSliderControl*	fPressureSlider;
			BCheckBox*				fRelative;
			BCheckBox* 			 	fAllLayersCheckbox;
};


#endif	// CLONE_TOOL_H
