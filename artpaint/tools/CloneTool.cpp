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

 #include "CloneTool.h"

#include "BitmapDrawer.h"
#include "BitmapUtilities.h"
#include "Brush.h"
#include "CoordinateQueue.h"
#include "CoordinateReader.h"
#include "Cursors.h"
#include "Image.h"
#include "ImageUpdater.h"
#include "ImageView.h"
#include "NumberSliderControl.h"
#include "PaintApplication.h"
#include "PixelOperations.h"
#include "ToolManager.h"
#include "ToolScript.h"
#include "UtilityClasses.h"


#include <Catalog.h>
#include <CheckBox.h>
#include <ClassInfo.h>
#include <File.h>
#include <GridLayoutBuilder.h>
#include <GroupLayoutBuilder.h>
#include <Layout.h>
#include <Window.h>


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Tools"


using ArtPaint::Interface::NumberSliderControl;


CloneTool::CloneTool()
	:
	DrawingTool(B_TRANSLATE("Clone tool"), "d", CLONE_TOOL)
{
	fOptions = SIZE_OPTION | USE_BRUSH_OPTION | PRESSURE_OPTION | MODE_OPTION
		| PREVIEW_ENABLED_OPTION;
	fOptionsCount = 5;

	SetOption(SIZE_OPTION, 1);
	SetOption(USE_BRUSH_OPTION, B_CONTROL_OFF);
	SetOption(PRESSURE_OPTION, 100);
	SetOption(MODE_OPTION, 0);
	SetOption(PREVIEW_ENABLED_OPTION, 0);

	origin.x = 0;
	origin.y = 0;
}


ToolScript*
CloneTool::UseTool(ImageView* view, uint32 buttons, BPoint point, BPoint viewPoint)
{
	// Wait for the last_updated_region to become empty
	while (LastUpdatedRect().IsValid())
		snooze(50000);

	image_view = view;
	CoordinateReader* coordinate_reader
		= new (std::nothrow) CoordinateReader(view, NO_INTERPOLATION, false);
	if (coordinate_reader == NULL)
		return NULL;

	ToolScript* the_script = new (std::nothrow)
		ToolScript(Type(), fToolSettings, ((PaintApplication*)be_app)->Color(true));
	if (the_script == NULL) {
		delete coordinate_reader;

		return NULL;
	}
	BBitmap* buffer = view->ReturnImage()->ReturnActiveBitmap();

	if (buffer == NULL) {
		delete coordinate_reader;
		delete the_script;

		return NULL;
	}
	BBitmap* srcBitmap;

	if (fToolSettings.preview_enabled == 0)
		srcBitmap = view->ReturnImage()->ReturnActiveBitmap();
	else
		srcBitmap = view->ReturnImage()->ReturnRenderedImage();

	BBitmap* srcBuffer = new (std::nothrow) BBitmap(srcBitmap);
	if (srcBuffer == NULL) {
		delete coordinate_reader;
		delete the_script;

		return NULL;
	}
	BBitmap* tmpBuffer = new (std::nothrow) BBitmap(buffer);
	if (tmpBuffer == NULL) {
		delete coordinate_reader;
		delete the_script;
		delete srcBuffer;
		return NULL;
	}
	union color_conversion clear_color;
	clear_color.word = 0xFFFFFFFF;
	clear_color.bytes[3] = 0x00;

	BitmapUtilities::ClearBitmap(tmpBuffer, clear_color.word);

	Selection* selection = view->GetSelection();
	BitmapDrawer* drawer = new (std::nothrow) BitmapDrawer(tmpBuffer);
	if (drawer == NULL) {
		delete coordinate_reader;
		delete the_script;
		delete srcBuffer;
		delete tmpBuffer;

		return NULL;
	}

	BPoint prev_point;

	prev_point = point;
	BRect updated_rect;
	Brush* brush;
	bool delete_brush = false;

	if (fToolSettings.use_current_brush == true)
		brush = ToolManager::Instance().GetCurrentBrush();
	else {
		brush_info default_free_brush;
		default_free_brush.shape = HS_ELLIPTICAL_BRUSH;
		default_free_brush.width = fToolSettings.size;
		default_free_brush.height = fToolSettings.size;
		default_free_brush.angle = 0;
		default_free_brush.hardness = 100;
		brush = new Brush(default_free_brush);
		delete_brush = true;
	}

	float brush_width_per_2 = floor(brush->Width() / 2);
	float brush_height_per_2 = floor(brush->Height() / 2);

	float pressure = (float)fToolSettings.pressure / 100.;

	ImageUpdater* imageUpdater = new ImageUpdater(view, 2000);

	brush->draw(tmpBuffer,
		BPoint(prev_point.x - brush_width_per_2, prev_point.y - brush_height_per_2), selection);

	// This makes sure that the view is updated even if just one point is drawn
	updated_rect.left = min_c(point.x - brush_width_per_2, prev_point.x - brush_width_per_2);
	updated_rect.top = min_c(point.y - brush_height_per_2, prev_point.y - brush_height_per_2);
	updated_rect.right = max_c(point.x + brush_width_per_2, prev_point.x + brush_width_per_2);
	updated_rect.bottom = max_c(point.y + brush_height_per_2, prev_point.y + brush_height_per_2);

	if (buttons == B_SECONDARY_MOUSE_BUTTON) {
		origin = point;
		imageUpdater->AddRect(BRect(point.x - 10, point.y - 10, 20, 20));
	} else if (origin_delta.x != 0 && origin_delta.y != 0 && fToolSettings.mode != false)
		origin = point + point_delta;

	BPoint orig_point = point;
	point_delta = origin - orig_point;
	origin_delta = point - orig_point;

	buffer->Lock();
	BitmapUtilities::CompositeBitmapOnSourceFromOffset(
				buffer, srcBuffer, tmpBuffer, updated_rect, src_over_fixed,
				point_delta, pressure);
	buffer->Unlock();

	SetLastUpdatedRect(updated_rect);
	the_script->AddPoint(point);

	imageUpdater->AddRect(updated_rect);

	while (coordinate_reader->GetPoint(point) == B_OK) {
		if (prev_point != point) {
			origin_delta = point - orig_point;
			the_script->AddPoint(point);
			brush->draw(tmpBuffer,
				BPoint(point.x - brush_width_per_2, point.y - brush_height_per_2), selection);
			brush->draw_line(tmpBuffer, point, prev_point, selection);

			updated_rect.left
				= min_c(point.x - brush_width_per_2 - 1, prev_point.x - brush_width_per_2 - 1);
			updated_rect.top
				= min_c(point.y - brush_height_per_2 - 1, prev_point.y - brush_height_per_2 - 1);
			updated_rect.right
				= max_c(point.x + brush_width_per_2 + 1, prev_point.x + brush_width_per_2 + 1);
			updated_rect.bottom
				= max_c(point.y + brush_height_per_2 + 1, prev_point.y + brush_height_per_2 + 1);

			updated_rect = updated_rect & buffer->Bounds();
			imageUpdater->AddRect(updated_rect);

			SetLastUpdatedRect(LastUpdatedRect() | updated_rect);

			buffer->Lock();
			BitmapUtilities::CompositeBitmapOnSourceFromOffset(
				buffer, srcBuffer, tmpBuffer, updated_rect, src_over_fixed,
				point_delta, pressure);
			buffer->Unlock();

			prev_point = point;
		}
	}

	imageUpdater->ForceUpdate();
	delete imageUpdater;

	delete srcBuffer;
	delete tmpBuffer;

	delete drawer;
	delete coordinate_reader;

	if (delete_brush == true)
		delete brush;

	return the_script;
}


int32
CloneTool::UseToolWithScript(ToolScript*, BBitmap*)
{
	return B_OK;
}


BView*
CloneTool::ConfigView()
{
	return new CloneToolConfigView(this);
}


const void*
CloneTool::ToolCursor() const
{
	return HS_FREE_LINE_CURSOR;
}


const char*
CloneTool::HelpString(bool isInUse) const
{
	return (isInUse ? B_TRANSLATE("Cloning part of the image.") :
		B_TRANSLATE("Clone a part of the image. Right-click to set origin."));
}


void
CloneTool::DrawTool(BView* view)
{
	ImageView* imgView = cast_as(view, ImageView);
	drawing_mode old_mode = imgView->DrawingMode();
	imgView->SetDrawingMode(B_OP_COPY);
	rgb_color high = imgView->HighColor();
	rgb_color low = imgView->LowColor();
	imgView->SetHighColor(255, 255, 255, 255);
	imgView->SetLowColor(0, 0, 0, 255);

	float scale = imgView->getMagScale();

	float size = max_c(1. / imgView->getScaleFactor(), 10.0);

	BPoint view_origin(origin.x * scale, origin.y * scale);
	BPoint view_delta((origin.x + origin_delta.x) * scale, (origin.y + origin_delta.y) * scale);
	BRect ellipse_rect = BRect(view_origin - BPoint(size, size), view_origin + BPoint(size, size));

	imgView->StrokeEllipse(ellipse_rect, B_MIXED_COLORS);
	imgView->StrokeLine(BPoint(view_origin.x, view_origin.y - size),
		BPoint(view_origin.x, view_origin.y + size), B_MIXED_COLORS);
	imgView->StrokeLine(BPoint(view_origin.x - size, view_origin.y),
		BPoint(view_origin.x + size, view_origin.y), B_MIXED_COLORS); //SOLID_HIGH);

	BRect crosshair_rect = BRect(view_delta - BPoint(size*2, size*2),
		view_delta + BPoint(size*2, size*2));

	imgView->StrokeLine(BPoint(view_delta.x, view_delta.y - size * 2),
		BPoint(view_delta.x, view_delta.y + size * 1.5), B_MIXED_COLORS);
	imgView->StrokeLine(BPoint(view_delta.x - size * 1.5, view_delta.y),
		BPoint(view_delta.x + size * 2, view_delta.y), B_MIXED_COLORS);

	imgView->Invalidate(ellipse_rect.InsetByCopy(-10, -10));
	imgView->Invalidate(crosshair_rect.InsetByCopy(-10, -10));

	imgView->SetDrawingMode(old_mode);
	imgView->SetHighColor(high);
	imgView->SetLowColor(low);
}


status_t
CloneTool::readSettings(BFile& file, bool is_little_endian)
{
	if (DrawingTool::readSettings(file, is_little_endian) != B_OK)
		return B_ERROR;

	float origin_x, origin_y;

	if (file.Read(&origin_x, sizeof(float)) != sizeof(float))
		return B_ERROR;

	if (file.Read(&origin_y, sizeof(float)) != sizeof(float))
		return B_ERROR;

	if (is_little_endian) {
		origin_x = B_LENDIAN_TO_HOST_FLOAT(origin_x);
		origin_y = B_LENDIAN_TO_HOST_FLOAT(origin_y);
	} else {
		origin_x = B_BENDIAN_TO_HOST_FLOAT(origin_x);
		origin_y = B_BENDIAN_TO_HOST_FLOAT(origin_y);
	}

	origin.x = origin_x;
	origin.y = origin_y;

	return B_OK;
}


status_t
CloneTool::writeSettings(BFile& file)
{
	if (DrawingTool::writeSettings(file) != B_OK)
		return B_ERROR;

	if (file.Write(&origin.x, sizeof(float)) != sizeof(float))
		return B_ERROR;

	if (file.Write(&origin.y, sizeof(float)) != sizeof(float))
		return B_ERROR;

	return B_OK;
}


// #pragma mark -- CloneToolConfigView


CloneToolConfigView::CloneToolConfigView(DrawingTool* tool)
	:
	DrawingToolConfigView(tool)
{
	if (BLayout* layout = GetLayout()) {
		BMessage* message = new BMessage(OPTION_CHANGED);
		message->AddInt32("option", SIZE_OPTION);
		message->AddInt32("value", tool->GetCurrentValue(SIZE_OPTION));

		fLineSize = new NumberSliderControl(B_TRANSLATE("Width:"), "1", message, 1, 100, false);

		BGridLayout* lineSizeLayout = LayoutSliderGrid(fLineSize);

		message = new BMessage(OPTION_CHANGED);
		message->AddInt32("option", USE_BRUSH_OPTION);
		message->AddInt32("value", 0x00000000);
		fUseBrush = new BCheckBox(B_TRANSLATE("Use current brush"), message);

		message = new BMessage(OPTION_CHANGED);
		message->AddInt32("option", PRESSURE_OPTION);
		message->AddInt32("value", 100);

		fPressureSlider
			= new NumberSliderControl(B_TRANSLATE("Pressure:"), "100", message, 1, 100, false);

		BGridLayout* pressureLayout = LayoutSliderGrid(fPressureSlider);

		message = new BMessage(OPTION_CHANGED);
		message->AddInt32("option", MODE_OPTION);
		message->AddInt32("value", 0x00000000);
		fRelative = new BCheckBox(B_TRANSLATE("Relative to cursor"), message);

		message = new BMessage(OPTION_CHANGED);
		message->AddInt32("option", PREVIEW_ENABLED_OPTION);
		message->AddInt32("value", 0x00000000);

		fAllLayersCheckbox = new BCheckBox(B_TRANSLATE("Sample all layers"), message);

		layout->AddView(BGroupLayoutBuilder(B_VERTICAL, kWidgetSpacing)
			.Add(lineSizeLayout)
			.Add(pressureLayout)
			.Add(fUseBrush)
			.Add(fRelative)
			.Add(fAllLayersCheckbox)
			.TopView()
		);

		fUseBrush->SetValue(tool->GetCurrentValue(USE_BRUSH_OPTION));
		if (tool->GetCurrentValue(USE_BRUSH_OPTION) != B_CONTROL_OFF)
			fLineSize->SetEnabled(FALSE);

		fPressureSlider->SetValue(tool->GetCurrentValue(PRESSURE_OPTION));

		fRelative->SetValue(tool->GetCurrentValue(MODE_OPTION));
		fAllLayersCheckbox->SetValue(tool->GetCurrentValue(PREVIEW_ENABLED_OPTION));
	}
}


void
CloneToolConfigView::AttachedToWindow()
{
	DrawingToolConfigView::AttachedToWindow();

	fLineSize->SetTarget(this);
	fUseBrush->SetTarget(this);
	fPressureSlider->SetTarget(this);
	fRelative->SetTarget(this);
	fAllLayersCheckbox->SetTarget(this);
}


void
CloneToolConfigView::MessageReceived(BMessage* message)
{
	DrawingToolConfigView::MessageReceived(message);

	switch (message->what) {
		case OPTION_CHANGED:
		{
			if (message->FindInt32("option") == USE_BRUSH_OPTION) {
				if (fUseBrush->Value() == B_CONTROL_OFF)
					fLineSize->SetEnabled(TRUE);
				else
					fLineSize->SetEnabled(FALSE);
			}
		} break;
	}
}
