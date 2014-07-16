Docks and Divs
==============

Here's what we try to do.
```
+----------------------+
|       Materials      |
|  __________________  |
|?(__________________)x|
|----------------------|
| /¨\                 #|
|(   )  Gold          #|
|(   )  Metal         #|
| \_/                 #|
|.....................#|
| /¨\                 #|
|(   )  Matte Plastic #|
|(   )  Phong         #|
| \_/                  |
|......................|
| /¨\                  |
|(   ) Glass           |
|----------------------|
|      (+Add) (Remove) |
+----------------------+
```

Here's how we do it:
```C
miPanelBegin(50,50, 250,450);

// Header
miDockBegin(MI_TOP_BOTTOM);
	miText("Materials");
	MIhandle searchInput, searchClear;
	float cols[3] = {MI_ICON_WIDTH, MI_FLEX, MI_ICON_WIDTH};
	miDivsBegin(MI_LEFT_RIGHT, 3, cols);
		miRowHeight(MI_INPUT_HEIGHT);
		miIcon(“search”);
		searchInput = miInput(searchText, sizeof(searchText));
		searchClear = miIcon(“cross”);
	miDivsEnd();
	if (miChanged(searchInput))
		setFilter(filter, searchText);
	if (miClicked(searchClear))
		strcpy(searchText, “”);
miDockEnd();

// Footer
miDockBegin(MI_BOTTOM_TOP);
	MIhandle add, del;
	float cols2[3] = {MI_FLEX, MI_ICONBUTTON_WIDTH, MI_BUTTON_WIDTH};
	miDivsBegin(MI_LEFT_RIGHT, 3, cols2);
		miRowHeight(MI_BUTTON_HEIGHT);
		miSpacer();
		add = miIconButton("Add”, “plus”);
		del = miButton("Delete");
	miDivsEnd();
	if (miClicked(add)) {
		selected = addMaterial();
	}
	if (miClicked(add)) {
		if (selected != -1)
			deleteMaterial(selected);
		selected = -1;
	}

miDockEnd();

// Scroll view
miDockBegin(MI_FILLY);
	miOverflow(MI_SCROLL);
	for (i = 0; i < materialCount; i++) {
		Material* mat = materials[i];
		if (!passFilter(filter, mat->name))
			continue;
		// Material
		miLayoutBegin(MI_LEFT_RIGHT);
			MIhandle item = miSelectableBegin(i == selected);
			miRowHeight(MI_THUMBNAIL_SIZE);
			miThumbnail(material->image);
			float rows[4] = {MI_FLEX, MI_TEXT_HEIGHT, MI_LABEL_HEIGHT, MI_FLEX};
			miDivsBegin(MI_TOP_BOTTOM, 4, rows);
				miSpacer();
				miText(mat->name);
				miText(mat->type);
			miDivsEnd();
			miSelectableEnd();
			if (miClicked(item)) {
				selected = i;
				setMaterialPreview(mat);
			}
		miLayoutEnd();
	}
miDockEnd();

miPanelEnd();
```


Docks
-----

A Dock arranges widgets in specified direction starting from the edge of the free space. Widgets are packed along the specified direction, and the space allocation stretches as new widgets are added. Each widget is given full width on the cross direction of the layout.

Let's take a look at the main structure of the dialog.

```C
// Header
miDockBegin(MI_TOP_BOTTOM);
	...
miDockEnd();

// Footer
miDockBegin(MI_BOTTOM_TOP);
	...
miDockEnd();

// Scroll view
miDockBegin(MI_FILLY);
	...
miDockEnd();
```

This divides the panel as follows:
```
+-----------+
| header  v |
|...........|
| scroll    |
|           |
|...........|
| footer  ^ | 
+-----------+ 
```
Initially the free space is the whole dialog. Header is docked at the top of the free space and widgets inside the header flow down, each widget taking a space that is full width of the available space and the height is determined by the widget content size. On miDockEnd(), the free space of the parent dialog is shrank based on the contents in the header. This space is available on further docks.

Footer is handled the same way, but the widgets grow upwards. 

If we used MI_LEFT_RIGHT on the footer, it would look on high level like this instead:

```
+-----------+
| header  v |
|...........|
|foo: scroll|
|>  :       |
|   :       |
|   :       | 
+-----------+ 
```


Divs
----

Divs divide the remaining free space into rows or columns; or divs. Widgets are assigned to one div at a time. When all the divs are full, new line is formed and divs filled again in order. The size of the divs can be fixed, or you can use MI_FLEX in which case the size is the remaining space divided equally across all flex elements. The div sizes are adjusted so that they always take the full space available.

```C
float cols2[3] = {MI_FLEX, MI_ICONBUTTON_WIDTH, MI_BUTTON_WIDTH};
miDivsBegin(MI_LEFT_RIGHT, 3, cols2);
	miRowHeight(MI_BUTTON_HEIGHT);
	miSpacer();
	miIconButton("Add”, “plus”);
	miButton("Delete");
miDivsEnd();
```
```
|                      |
|......................|
|        : +Add : Del  |
+----------------------+ 
: flex   : ibut : but  :
```
In this example the buttons are right aligned using a flex and spacer. Note that by default the divs fill the whole free space horizontally and vertically. You can use row height and column width to limit the size of the row.

If we added one more button to the above layout, it would look like this:

```
|                      |
|......................|
| More   :             |
|......................|
|        : +Add : Del  |
+----------------------+ 
```

Since the parent dock was flowing, up the new line added follows that convention too.
