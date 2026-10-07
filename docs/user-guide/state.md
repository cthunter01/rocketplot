# Saving and restoring state

[User Guide](index.md) · previous: [Export](export.md) · next: [Qt Designer](designer.md)

Users set a plot up the way they need it: they zoom to the part they care about, hide the channels they don't,
drag the legend out of the way, make one line red. `saveState()` captures that, and `restoreState()` brings it
back, in the next session or on another plot of the same data.

## State and content

The key idea is the split between what a plot *shows* and how it is *set up to show it*.

| Part of the state | Not part of it (your code provides it) |
| --- | --- |
| The theme mode; whether the crosshair is on, and what it follows | The data |
| Each axis: range; autoscale, its mode, margin and follow window; scale type, number format and time zone; grid, minor grid, visibility, tick labels | The series' names |
| The legend: visibility, anchor, position, values at the crosshair | The title and the axis labels |
| Each series: visibility, color, marker and its size, line width and style, error style, cap size and band opacity, y axis | The annotations |
| | The input bindings; a custom theme's colors |

So the order of things is: your code builds the plot (adds the series, sets titles and labels, adds annotations),
then restores the state on top.

## Saving and restoring

The state is a `QJsonObject`. Keep it wherever you keep settings. Here `settings` is the application's
`QSettings`, and the state goes into it as compact JSON text.

<!-- example: state-save -->
```cpp
// When the window closes: how the user left the plot.
const QJsonObject state = plot->saveState();
settings.setValue("plots/chillDown", QJsonDocument(state).toJson(QJsonDocument::Compact));
```

<!-- example: state-restore -->
```cpp
// When it opens again, after the plot has its series.
const QByteArray text = settings.value("plots/chillDown").toByteArray();
if (!plot->restoreState(QJsonDocument::fromJson(text).object()))
{
    // Nothing was saved yet, or by a version of the library this one can't read: the plot
    // is as the code set it up.
}
```

`restoreState()` returns `false`, and changes nothing, when the object isn't a state this version of the library
can read: an empty object (nothing was saved yet), something else entirely, or a state written by a newer version
with a format this one doesn't know.

How a restore treats each part:

- **Series are matched by name.** A series gets the settings saved for the series of the same name. Where several
  have the same name, they are matched in order. Series the state doesn't mention are left as they are, and saved
  settings for series that no longer exist are ignored. This is why a state survives the data changing: add a
  channel, and the others still come back as the user left them.
- **An axis that was autoscaling** fits the data the plot has *now*. An axis with a fixed range gets that range
  back.
- **Settings that followed the theme keep following it.** A series color nobody chose is saved as "the theme's",
  not as the color it happened to be, so it is still right after a switch to dark mode.
- **The view history is not touched**: a restore is not a step the user can go back from, like ranges set in code.

## What a state looks like

A plot of two series, after the user zoomed in, hid one series and recolored the other:

<!-- output: state-json -->
```json
{
    "crosshair": false,
    "crosshairMode": "FREE",
    "format": "rocketplot.state",
    "legend": {
        "anchor": "TOP_RIGHT",
        "position": [
            1,
            0
        ],
        "valuesVisible": true,
        "visible": null
    },
    "series": [
        {
            "bandOpacity": null,
            "color": "#c2410c",
            "errorCapSize": null,
            "errorStyle": "BAND",
            "lineStyle": "SolidLine",
            "lineWidth": null,
            "marker": "NONE",
            "markerSize": null,
            "name": "LOX feed line",
            "onSecondaryYAxis": false,
            "visible": true
        },
        {
            "bandOpacity": null,
            "color": null,
            "errorCapSize": null,
            "errorStyle": "BAND",
            "lineStyle": "SolidLine",
            "lineWidth": null,
            "marker": "NONE",
            "markerSize": null,
            "name": "Tank wall",
            "onSecondaryYAxis": false,
            "visible": false
        }
    ],
    "themeMode": "SYSTEM",
    "version": 1,
    "xAxis": {
        "autoscale": false,
        "autoscaleMargin": 0.03,
        "autoscaleMode": "FIT_ALL",
        "followWindow": 10,
        "gridVisible": true,
        "max": 400,
        "min": 100,
        "minorGridVisible": false,
        "numberFormat": "AUTO",
        "scaleType": "LINEAR",
        "tickLabelsVisible": true,
        "timeZone": "UTC",
        "visible": null
    },
    "yAxis": {
        "autoscale": false,
        "autoscaleMargin": 0.03,
        "autoscaleMode": "FIT_ALL",
        "followWindow": 10,
        "gridVisible": true,
        "max": -40,
        "min": -170,
        "minorGridVisible": false,
        "numberFormat": "AUTO",
        "scaleType": "LINEAR",
        "tickLabelsVisible": true,
        "timeZone": "UTC",
        "visible": null
    },
    "yAxis2": {
        "autoscale": true,
        "autoscaleMargin": 0.03,
        "autoscaleMode": "FIT_ALL",
        "followWindow": 10,
        "gridVisible": false,
        "max": 1,
        "min": 0,
        "minorGridVisible": false,
        "numberFormat": "AUTO",
        "scaleType": "LINEAR",
        "tickLabelsVisible": true,
        "timeZone": "UTC",
        "visible": null
    }
}
```

The text is meant to be readable, and stable: enumerations are written by name and colors as `#rrggbb` (or
`#aarrggbb` when translucent). `null` stands for "not set": the value follows the theme (a color, a line width)
or the default rule (the legend shows from two entries on, the right axis when a series uses it). `format` and
`version` identify the text as a state and say which layout it has.

## Restoring only part of it

Keys that are missing from a state are left alone by a restore. Take out what you don't want to bring back.

<!-- example: state-partial -->
```cpp
QJsonObject view = plot->saveState();
view.remove("themeMode");  // the theme stays whatever it is when the state is restored
view.remove("series");     // and so does how each series is drawn
plot->restoreState(view);  // the axes, the legend and the crosshair
```

The top-level keys are `themeMode`, `crosshair`, `crosshairMode`, `xAxis`, `yAxis`, `yAxis2`, `legend` and
`series`. This is also how to copy only the view from one plot to another: save the first, keep the axis keys,
restore on the second.

## A grid's state

`PlotGrid::saveState()` holds the state of every plot of the grid, row by row, and `restoreState()` applies them
in the same order, as far as both go. See [Several plots](multiple-plots.md#exporting-and-saving-a-grid).

## When to save

Qt has no "about to close" signal on a plot. Save where you save your other settings: in the window's
`closeEvent()`, or when the user saves a document that the plot is part of. Restore after the plot has its series,
because series are matched by name and a series that isn't there yet gets nothing.

Next: [Qt Designer](designer.md).
