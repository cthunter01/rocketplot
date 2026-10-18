# The legend

[User Guide](index.md) · previous: [Interaction](interaction.md) · next: [Several plots](multiple-plots.md)

The legend lists the plot's named series: a sample of how each is drawn, and its name. It can also list
annotations, and give several series one entry. It sits inside the plot area and is also where the user turns
series on and off.

![Three lines and their legend](images/legend.png)

There is one legend per plot, there from the start: `plot->legend()`.

## Which series are in it

Every series with a name has an entry, in the order the series were added. A series without a name has none: use
this for series that need no explanation, such as a faint trace of raw samples under a smoothed line.

`series->setName()` changes an entry; an empty name removes it.

### Several series, one entry

Series that stand for one quantity in several parts (the stages of a flight, the runs of a test) can share one
entry: give them the same legend group. The entry has the name and the look of the group's first named series,
and clicking it, double-clicking it, pointing at it and its menu act on every series of the group. Give them one
color and tell them apart by line style or marker, as below.

![Two runs of two sensors: two entries, each for a solid and a dashed line, and an entry for a target line](images/legend-groups.png)

<!-- example: legend-groups -->
```cpp
// Two runs of the same test: one entry per sensor for both, the runs told apart by line style.
for (const Run& run : runs)
{
    rocketplot::LineSeries* feed = plot->addLine(time, run.feed, "LOX feed line");
    feed->setLegendGroup("feed");
    feed->setColor(QColor("#2a78d6"));
    feed->setLineStyle(run.style);
    rocketplot::LineSeries* housing = plot->addLine(time, run.housing, "Turbopump housing");
    housing->setLegendGroup("housing");
    housing->setColor(QColor("#e8743b"));
    housing->setLineStyle(run.style);
}

// An annotation can have an entry too, after the series'.
plot->addHorizontalLine(-150.0)->setLegendName("Feed line target");
```

### Annotations

An annotation with a legend name (`setLegendName()`) has an entry after the series': a sample of the line, the
shaded span, the rectangle, the flag or icon, or the text box. Clicking it hides or shows the annotation, and its
menu has Visible, Color…, Theme color and Remove (and `annotationEntryMenuAboutToShow()` to add to it). It shows
no value at the crosshair.

## When it shows

By default the legend appears once it has two entries. A plot with a single series is better served by a title
or an axis label that names it.

## Where it goes

The legend starts at `LegendAnchor::BEST`: the corner or edge of the plot area where it covers the least data. It
doesn't jump around as data changes: it moves only when another place is clearly better.

<!-- example: legend-place -->
```cpp
rocketplot::Legend* legend = plot->legend();
legend->setAnchor(rocketplot::LegendAnchor::BOTTOM_RIGHT);  // a corner or the middle of an edge
legend->setAnchor(rocketplot::LegendAnchor::BEST);          // the default: where it hides least

// Anywhere: fractions of the room the plot area leaves around the legend. (0, 0) is the top
// left corner, (1, 1) the bottom right. This is what dragging the legend sets.
legend->setPosition({0.5, 0.0});
```

The anchors are the four corners and the four edges: `TOP_LEFT`, `TOP`, `TOP_RIGHT`, `RIGHT`, `BOTTOM_RIGHT`,
`BOTTOM`, `BOTTOM_LEFT`, `LEFT`. A legend placed by `setPosition()`, or dragged by the user, has the anchor
`CUSTOM`. Because the position is a fraction and not a pixel count, the legend stays inside the plot area when
the window is resized.

### Outside the plot area

A legend of many entries covers the data wherever it goes inside the plot area. Two anchors put it outside, and
the plot area makes room for it:

![Five lines and their legend under the plot, its entries in rows](images/legend-outside.png)

<!-- example: legend-outside -->
```cpp
// Under the plot, outside the plot area: the entries run across in rows.
plot->legend()->setAnchor(rocketplot::LegendAnchor::OUTSIDE_BOTTOM);
```

| Anchor | Where | Its entries |
| --- | --- | --- |
| `OUTSIDE_BOTTOM` | Under the plot, centered under the plot area | Across in rows, as many to a row as fit the width; rows that would take more than 40% of the height are left out |
| `OUTSIDE_RIGHT` | Right of the plot area, from its top | Down in columns as tall as the plot area; names are shortened where the columns would take more than 40% of the width |

An outside legend has no box (the plot area's edge sets it apart), can't be dragged, and doesn't move with the
pointer: while the crosshair shows values, its value column is as wide as the widest value of its series, so the
plot area doesn't change size as the pointer moves. Clicking, pointing and its menu work as inside the plot area,
and the plots of a grid stay lined up. `BEST` never puts the legend outside.

## What the user can do with it

| Gesture on an entry | What it does |
| --- | --- |
| Click | Hides the series, or shows it again. The entry stays, dimmed |
| Double-click | Shows only this series (or group); again to show them all |
| Point at it | Brings the series forward and fades the others, to pick one line out of many |
| Right-click | A menu for this series: visible, show only this, show all, color, line width, marker, remove |
| Drag (anywhere on the legend) | Moves the legend |

![The same plot with the pointer on the second entry: its line stands out, the others are faded](images/legend-highlight.png)

Hiding a series changes what autoscale fits: hide the one series with large values and the others fill the plot.

**Remove** in an entry's menu deletes the series. If your code keeps pointers to series, connect to the plot's
`seriesRemoved(series)` signal and forget the pointer there, or turn the legend's interaction off.

What the user changes here (hidden series, a color, the legend's place) is part of the plot's state, which you
can save and restore; see [Saving and restoring state](state.md).

## Values at the crosshair

With the plot's crosshair on, each entry shows its series' value at the pointer; see
[Interaction](interaction.md#the-crosshair). When the crosshair follows the data and is on a point, the entry
of that point's series shows the value of the point itself. The entry of a legend group shows the value of the
series the crosshair is on, or else of the first of its series that has one there.

## Settings

<!-- example: legend-options -->
```cpp
legend->setVisible(true);   // show it even for a single series
legend->setVisible(false);  // never show it
legend->resetVisible();     // the default: shown for two or more named series

legend->setValuesVisible(false);  // no values at the crosshair
legend->setInteractive(false);    // no clicking, pointing, dragging or menu
```

A legend that isn't interactive is drawn the same; it only ignores the pointer, so drags over it pan the plot.

## Adding to an entry's menu

The menu that a right-click on an entry opens can take entries of your own.

<!-- example: legend-menu -->
```cpp
QObject::connect(legend, &rocketplot::Legend::entryMenuAboutToShow, plot,
                 [](QMenu* menu, rocketplot::Series* series) {
                     menu->addSeparator();
                     menu->addAction("Copy name", series, [series] {
                         QGuiApplication::clipboard()->setText(series->name());
                     });
                 });
```

The signal comes with the series the entry is for. As with the plot's context menu, the menu is new each time it
opens.

Next: [Several plots](multiple-plots.md).
