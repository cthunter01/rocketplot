# Export

[User Guide](index.md) · previous: [Appearance](appearance.md) · next: [Saving and restoring state](state.md)

A plot can leave the application as an image, as a drawing (SVG or PDF), on the clipboard, on a printed page, or
as the numbers it shows (CSV).

## What an export shows

An export is the plot as the widget shows it: the same view, series, legend and annotations. Left out is what
follows the pointer: the crosshair, the zoom box, and the highlight of a legend entry being pointed at.

The user gets two exports without any code, from the context menu: **Copy image**, and **Export…**, which asks
for a file name and writes the format its suffix names.

## Writing files

In the examples of this chapter, `folder` is a `QDir`: the folder the files go to.

<!-- example: export-files -->
```cpp
// The suffix picks the format.
for (const QString name : {"ascent.png", "ascent.svg", "ascent.pdf", "ascent.csv"})
{
    if (!plot->exportTo(folder.filePath(name)))
    {
        qWarning() << "could not write" << name;
    }
}
```

| Function | Writes |
| --- | --- |
| `exportTo(fileName)` | Whatever the suffix says: `.svg`, `.pdf`, `.csv`, `.tsv`, or an image format |
| `exportImage(fileName)` | An image, in any format Qt can write: `.png`, `.jpg`, `.bmp`, ... |
| `exportSvg(fileName)` | An SVG drawing |
| `exportPdf(fileName)` | A PDF document of one page, the size of the plot |
| `exportCsv(fileName)` | The data in view, as text |

All of them return `false` when the file can't be written. They are marked `[[nodiscard]]`: the compiler warns if
you ignore the result.

**Images or drawings?** A PNG is right for the web, for chat and for slides. SVG and PDF are drawings: they stay
sharp at any size, their text stays text (it can be searched, selected and restyled), and they are what a paper
or a report wants. Dense data is thinned in a drawing to what a screen would show, so a line of ten million points
doesn't make a file of ten million segments.

## Size, sharpness and theme

`ExportOptions` says how an export differs from the widget. Every field can be left out.

<!-- example: export-options -->
```cpp
// Laid out as a 600 × 400 widget would be, at 300 pixels per inch: 1875 × 1250 pixels.
const bool imageWritten =
    plot->exportImage(folder.filePath("figure.png"), {.size = {600, 400}, .dpi = 300.0});

// From a dark window onto white paper.
const bool pageWritten = plot->exportPdf(
    folder.filePath("figure.pdf"), {.size = {600, 400}, .theme = rocketplot::Theme::print()});
```

| Field | Meaning | Left out |
| --- | --- | --- |
| `size` | The size the plot is laid out in, in device-independent pixels (96 to the inch), as a widget's size is | The widget's own size |
| `dpi` | For images: pixels per inch | As sharp as the screen the widget is on |
| `theme` | The theme to draw with | The plot's own theme |

`size` and `dpi` do different things, and it helps to keep them apart:

- **`size` decides the layout**: how much room the data has next to the text. A plot exported at 600 × 400 looks
  like a 600 × 400 widget, with text the size it has on screen.
- **`dpi` decides the sharpness**: how many pixels that layout is drawn with. At 96 dpi a 600 × 400 plot is an
  image of 600 × 400 pixels. At 300 dpi it is 1875 × 1250 pixels, and looks the same, only sharper.

For a figure in a document, choose `size` so that the text reads well at the width the figure will be printed at
(600 pixels across is 6.25 inches at 96 to the inch), then choose the `dpi` the printer wants.

The `theme` applies to this export alone; the plot on screen stays as it is. `Theme::print()` turns a plot from
a dark window into a figure for white paper.

## In memory and on the clipboard

<!-- example: export-image -->
```cpp
const QImage image =
    plot->renderToImage({.size = {480, 300}, .dpi = 96.0});  // 480 × 300 pixels
plot->copyToClipboard();  // what "Copy image" in the context menu does
```

`renderToImage()` returns a null image if the image would be too large to make.

## On a page of your own

`paint()` draws the plot into a rectangle of any `QPainter`: a page being printed, a PDF with several plots and
text, a report widget.

<!-- example: export-paint -->
```cpp
// A PDF page with two plots, one above the other.
QPdfWriter writer(folder.filePath("report.pdf"));
writer.setPageSize(QPageSize(QPageSize::A4));
writer.setPageMargins(QMarginsF(15.0, 15.0, 15.0, 15.0), QPageLayout::Millimeter);
writer.setResolution(96);  // one unit of the painter is one device-independent pixel
QPainter     painter(&writer);
const double width  = writer.width();
const double height = writer.height() / 2.0;
plot->paint(painter, QRectF(0.0, 0.0, width, height));
velocityPlot->paint(painter, QRectF(0.0, height, width, height));
painter.end();
```

The rectangle is in the painter's coordinates, and the plot is laid out as a widget of that size would be. With a
printer or a PDF writer, set the device's resolution so that one unit is about a screen pixel (as above), or the
text will come out tiny on a device that counts 1200 units to the inch.

For `QPrinter` it is the same: make a `QPainter` on the printer and call `paint()`.

## The data as CSV

`toCsv()` gives the data in view as text; `exportCsv()` writes it to a file.

<!-- example: export-csv -->
```cpp
plot->xAxis()->setRange(1.0, 4.0);  // only what is in view is written
const QString text = plot->toCsv();
```

For a plot of two series over the same six times, this gives:

<!-- output: export-csv -->
```text
Time (s),Altitude (m),Velocity (m/s)
1,0.4,8.1
2,1.7,17.9
3,3.9,26.2
4,6.8,33
```

What is written:

- **Only what is in view**: the points of the visible series whose x is within the x axis's range. Zoom to the
  part you want, then export.
- **A header row**: the x axis's label, then the names of the series. Rich-text markup is taken out.
- **Shared x columns**: series with the same x values (channels on one time base) share one x column. A series
  with other x values gets an x column of its own, headed `<x label> (<series>)`.
- **Errors** as two more columns per series and axis, `<series> (low)` and `<series> (high)`: the ends of the
  bars.
- **Numbers in full**: the shortest text that reads back as the same `double`, with a decimal point whatever the
  locale. A date/time axis is written as seconds since 1970, as it is stored.
- **Gaps** (NaN) as empty cells.
- Names that contain the separator, a quote or a line break are quoted (RFC 4180). Lines end with `\n`, and the
  text is UTF-8.

<!-- example: export-csv-file -->
```cpp
const bool csvWritten = plot->exportCsv(folder.filePath("ascent.csv"));        // to a file
const bool tsvWritten = plot->exportCsv(folder.filePath("ascent.tsv"), '\t');  // with tabs
```

`exportCsv()` writes straight to the file without building the whole text in memory first, which matters for
millions of points.

## Exporting several plots as one

A `PlotGrid` has the same functions and writes all its plots as one figure, lined up as on screen:
`grid->exportTo("flight.pdf")`. See [Several plots](multiple-plots.md#exporting-and-saving-a-grid).

## A note on SVG viewers

The data of an SVG export is clipped to the plot area with a clip path. Browsers and drawing programs (Inkscape,
Illustrator) honor it. Qt's own SVG renderer (`QSvgRenderer`, and so Qt's image viewer plugins) does not: there,
lines and markers at the edge of the plot run a few pixels past the axes. The file is fine; open it in a browser
to check.

Next: [Saving and restoring state](state.md).
