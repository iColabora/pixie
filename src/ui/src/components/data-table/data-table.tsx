/*
 * Copyright 2018- The Pixie Authors.
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */

import * as React from 'react';
import {
  useTable,
  useBlockLayout,
  useResizeColumns,
  useSortBy,
  Column,
  Cell,
  ColumnInstance, TableInstance,
} from 'react-table';
import { FixedSizeList as List, areEqual, ListOnItemsRenderedProps } from 'react-window';
import { makeStyles, Theme } from '@material-ui/core/styles';
import { createStyles } from '@material-ui/styles';
import DownIcon from '@material-ui/icons/KeyboardArrowDown';
import UpIcon from '@material-ui/icons/KeyboardArrowUp';
import { useScrollbarSize } from 'app/utils/use-scrollbar-size';
import { buildClass } from 'app/utils/build-class';

// Augments `@types/react-table` to recognize the plugins we're using
import './react-table-config.d';
import { AutoSizerContext, withAutoSizerContext } from 'app/utils/autosizer';
import {
  alpha, Button, Checkbox, FormControlLabel, Menu, MenuItem,
} from '@material-ui/core';
import { UnexpandedIcon } from 'app/components/icons/unexpanded';
import MenuIcon from '@material-ui/icons/Menu';

export type CellAlignment = 'center' | 'start' | 'end' | 'fill';

export interface ReactTable<D extends Record<string, any> = Record<string, any>> {
  columns: Array<Column<D>>;
  data: D[];
}

const ROW_HEIGHT_PX = 40;

const useDataTableStyles = makeStyles((theme: Theme) => createStyles({
  // Note: most display and overflow properties here are to align interactions between react-window and react-table.
  table: {
    display: 'block',
    fontSize: theme.typography.pxToRem(15),
    overflow: 'hidden',
  },
  tableHead: {
    overflow: 'hidden',
    height: `${ROW_HEIGHT_PX}px`,
  },
  headerRow: {
    display: 'flex',
    maxHeight: '100%',
    width: '100%',
  },
  headerCell: {
    position: 'relative', // In case anything inside positions absolutely
    fontSize: theme.typography.pxToRem(14),
    padding: theme.spacing(1),
    alignSelf: 'baseline',
    borderRight: '1px solid transparent',
    '&:last-child': {
      borderRightWidth: 0,
    },
  },
  headerCellContents: {
    display: 'flex',
    flexFlow: 'row nowrap',
    alignItems: 'center',
  },
  // Separate so that resize handles can clip visibly to the side (centering themselves on the border)
  headerLabel: {
    textTransform: 'uppercase',
    overflow: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    fontWeight: 500,
    display: 'inline-block',
    maxWidth: '100%',
    flexGrow: 1,
  },
  headerLabelRight: {
    textAlign: 'right',
  },
  // The body is pushed down by the height of the (sticky-positioned) header. react-window already accounts for this.
  tableBody: {
    position: 'absolute',
  },
  bodyRow: {
    '&:not(:last-child)': {
      borderBottom: `1px solid ${theme.palette.background.three}`,
    },
  },
  bodyRowSelectable: {
    cursor: 'pointer',
    '& .rowSelectionIcon': { opacity: 0 },
    '&:hover': {
      backgroundColor: `${alpha(theme.palette.foreground.grey2, 0.42)}`,
      '& .rowSelectionIcon': { opacity: 0.8 },
    },
  },
  bodyRowSelected: {
    backgroundColor: theme.palette.foreground.grey3,
    '& .rowSelectionIcon': { opacity: 1 },
  },
  bodyCell: {
    position: 'relative', // In case anything inside positions absolutely
    display: 'flex',
    alignItems: 'center',
    padding: `0 ${theme.spacing(1)}`,
    height: `${ROW_HEIGHT_PX}px`, // Ensures the border stretches. See cellContents for the rest.
    borderRight: `1px solid ${theme.palette.background.three}`,
    '&:last-of-type': {
      borderRightWidth: 0,
    },
  },
  gutterCell: {
    padding: 0,
    display: 'flex',
    justifyContent: 'center',
    alignItems: 'center',
    borderRightWidth: 0,
  },
  cellContents: {
    display: 'inline-block',
    overflow: 'hidden',
    whiteSpace: 'nowrap',
    textOverflow: 'ellipsis',
    maxHeight: '100%',
    width: '100%',
    lineHeight: `${ROW_HEIGHT_PX}px`,
  },
  start: { textAlign: 'left' },
  end: { textAlign: 'right' },
  center: { textAlign: 'center' },
  fill: { textAlign: 'center' },
  sortButton: {
    opacity: 0.2,
    width: theme.spacing(3),
    paddingLeft: theme.spacing(1),
  },
  sortButtonActive: {
    opacity: 1,
  },
  resizeHandle: {
    userSelect: 'none',
    position: 'absolute',
    right: '0',
    top: 'calc(50% - 2px)',
    transform: 'translate(50%, -50%)',
  },
  resizeHandleActive: {
    color: theme.palette.foreground.three,
    fontWeight: 'bold',
  },
  rowExpandButton: {
    display: 'flex',
    alignItems: 'center',
    height: '100%',
  },
  columnSelector: {
    position: 'relative',
    top: '-2px', // The labels and icons don't quite line up otherwise due to how text renders
    left: '50%',
    transform: 'translate(-50%)',
    width: theme.spacing(3),
    height: theme.spacing(3),
  },
}), { name: 'DataTable' });

export interface DataTableProps {
  table: ReactTable;
  enableColumnSelect?: boolean;
  enableRowSelect?: boolean;
  onRowSelected?: (row: Record<string, any>|null) => void;
  onRowsRendered?: (rendered: ListOnItemsRenderedProps) => void;
}

interface DataTableContextProps extends Omit<DataTableProps, 'table'> {
  instance: TableInstance;
  expanded: string;
  toggleRowExpanded: (rowId: string) => void;
}
const DataTableContext = React.createContext<DataTableContextProps>(null);

const noPointerEvents = { pointerEvents: 'none' as const };

const ColumnSelector = React.memo<{ columns: ColumnInstance[] }>(function ColumnSelector({ columns }) {
  const classes = useDataTableStyles();

  const [open, setOpen] = React.useState(false);
  const close = React.useCallback(() => setOpen(false), []);
  const toggleOpen = React.useCallback(() => setOpen((prev) => !prev), []);
  const anchorEl = React.useRef<HTMLButtonElement>(null);

  // Workaround: since react-table directly mutates its objects, column.isVisible on individual columns doesn't cause
  // React to update this component. Monitoring the instance state with useEffect is sufficient to trigger an update.
  const { hiddenColumns } = React.useContext(DataTableContext).instance.state;
  React.useEffect(() => {}, [hiddenColumns.length]);

  const editableColumns = React.useMemo(() => columns.filter((col) => !col.isGutter), [columns]);
  return (
    <>
      <Menu open={open} anchorEl={anchorEl.current} onBackdropClick={close}>
        {editableColumns.map((column) => (
          // eslint-disable-next-line react-memo/require-usememo
          <MenuItem key={column.id} button onClick={() => column.toggleHidden()}>
            <FormControlLabel
              style={noPointerEvents}
              label={column.id || JSON.stringify(column)}
              // eslint-disable-next-line react-memo/require-usememo
              control={<Checkbox color='secondary' disableRipple checked={column.isVisible} />}
            />
          </MenuItem>
        ))}
      </Menu>
      <Button className={classes.columnSelector} onClick={toggleOpen} ref={anchorEl}>
        <MenuIcon />
      </Button>
    </>
  );
});

const ColumnSortButton = React.memo<{ column: ColumnInstance }>(function ColumnSortButton({ column }) {
  const classes = useDataTableStyles();
  const className = buildClass(
    classes.sortButton,
    column.isSorted && classes.sortButtonActive,
  );
  return column.isSortedDesc ? <DownIcon className={className} /> : <UpIcon className={className} />;
});

const ColumnResizeHandle = React.memo<{ column: ColumnInstance }>(function ColumnResizeHandle({ column }) {
  const classes = useDataTableStyles();
  const { instance: { resetResizing } } = React.useContext(DataTableContext);

  return (
    <span
      {...column.getResizerProps()}
      onDoubleClick={resetResizing}
      className={buildClass(classes.resizeHandle, column.isResizing && classes.resizeHandleActive)}
    >
      &#8942;
    </span>
  );
});

const HeaderCell: React.FC<{ column: ColumnInstance }> = React.memo(function HeaderCell({ column }) {
  const classes = useDataTableStyles();

  const cellClass = buildClass(classes.headerCell, column.isGutter && classes.gutterCell);
  const contClass = buildClass(classes.headerCellContents, classes[column.align]);
  const labelClass = buildClass(classes.headerLabel, column.align === 'end' && classes.headerLabelRight);

  const sortProps = React.useMemo(() => (
    column.canSort ? column.getSortByToggleProps() : {}
    // eslint-disable-next-line react-hooks/exhaustive-deps
  ), [column.canSort]);

  return (
    // eslint-disable-next-line react/jsx-key
    <div {...column.getHeaderProps()} className={cellClass}>
      <div
        {...sortProps}
        title={column.id === 'controls' ? 'Select visible columns' : column.id}
        className={contClass}
      >
        <span className={labelClass}>
          {column.render('Header')}
        </span>
        {column.canSort && <ColumnSortButton column={column} />}
      </div>
      {column.canResize && <ColumnResizeHandle column={column} />}
    </div>
  );
}, (prev, next) => {
  const checkKeys: Array<keyof ColumnInstance> = [
    'id', 'Header', 'width', 'canSort', 'isSorted', 'isSortedDesc', 'canResize', 'isResizing'];
  for (const key of checkKeys) {
    if (prev.column[key] !== next.column[key]) return false;
  }
  return true;
});

const HeaderRow = React.memo(React.forwardRef<HTMLDivElement, { scrollbarWidth: number }>(({ scrollbarWidth }, ref) => {
  const classes = useDataTableStyles();
  const { width: containerWidth } = React.useContext(AutoSizerContext);
  const { instance: { totalColumnsWidth, flatHeaders } } = React.useContext(DataTableContext);

  const headStyle = React.useMemo(() => ({
    width: `${containerWidth - scrollbarWidth}px`,
  }), [containerWidth, scrollbarWidth]);
  const rowStyle = React.useMemo(() => ({
    width: `${totalColumnsWidth + scrollbarWidth}px`,
  }), [totalColumnsWidth, scrollbarWidth]);

  return (
    <div className={classes.tableHead} style={headStyle} ref={ref}>
      <div role='row' className={classes.headerRow} style={rowStyle}>
        {flatHeaders.filter((c) => c.isVisible).map((column) => (
          // eslint-disable-next-line react-memo/require-usememo
          <HeaderCell key={String(column.id || column.Header)} column={{ ...column }} />
        ))}
      </div>
    </div>
  );
}));
HeaderRow.displayName = 'HeaderRow';

const BodyCell: React.FC<{ cell: Cell }> = React.memo(function BodyCell({ cell }) {
  const classes = useDataTableStyles();
  const { column: col } = cell;

  const cellClass = buildClass(classes.bodyCell, col.isGutter && classes.gutterCell);
  const contClass = buildClass(classes.cellContents, classes[col.align]);
  const cellWidth = Math.max(col.minWidth ?? 0, Math.min(Number(col.width), col.maxWidth ?? Infinity));
  return (
    <div role='cell' className={cellClass} style={{ width: `${cellWidth}px` }}>
      <div className={contClass}>
        {cell.render('Cell')}
      </div>
    </div>
  );
}, (prev, next) => {
  const pCol = prev.cell.column;
  const nCol = next.cell.column;
  return pCol.id === nCol.id
    && pCol.Header === nCol.Header
    && pCol.width === nCol.width
    && prev.cell.value === next.cell.value;
});

const BodyRow = React.memo<{ index: number, style: React.CSSProperties }>(
  function BodyRow({
    index: rowIndex,
    style: vRowStyle,
  }) {
    const classes = useDataTableStyles();
    const {
      instance: {
        rows,
        totalColumnsWidth,
        prepareRow,
      },
      expanded,
      toggleRowExpanded,
      enableRowSelect,
      onRowSelected,
    } = React.useContext(DataTableContext);

    const row = rows[rowIndex];
    prepareRow(row);
    const className = buildClass(
      classes.bodyRow,
      enableRowSelect && classes.bodyRowSelectable,
      enableRowSelect && expanded === row.id && classes.bodyRowSelected,
    );
    const onClick = React.useMemo(() => enableRowSelect && ((event) => {
      // The canvas in QuantilesBoxWhisker takes clicks (through Vega), which it doesn't prevent from bubbling.
      // Let those clicks do just one thing, not two.
      if (event.target.tagName !== 'CANVAS') {
        toggleRowExpanded(row.id);
        onRowSelected?.(expanded === row.id ? null : row.original);
      }
    }), [row.id, row.original, enableRowSelect, onRowSelected, expanded, toggleRowExpanded]);

    const rowProps = React.useMemo(
      () => row.getRowProps({ style: { ...vRowStyle, width: totalColumnsWidth } }),
      [row, vRowStyle, totalColumnsWidth]);
    return (
      // eslint-disable-next-line react/jsx-key
      <div {...rowProps} className={className} onClick={onClick}>
        {/* eslint-disable-next-line react-memo/require-usememo */}
        {row.cells.map((cell) => <BodyCell key={cell.column.id} cell={{ ...cell, column: { ...cell.column } }} />)}
      </div>
    );
  },
  areEqual,
);

function decorateTable({ table: { columns, data }, enableColumnSelect, enableRowSelect }: DataTableProps): ReactTable {
  // Enabling row select will add icon indicators, but only if something else gives a reason to show a controls column.
  if (enableColumnSelect && !columns.some((col) => col.id === 'controls')) {
    columns.unshift({
      Header: function ControlHeader({ columns: columnInstances }) {
        return enableRowSelect ? <ColumnSelector columns={columnInstances} /> : <></>;
      },
      Cell: function ControlCell() {
        const classes = useDataTableStyles();
        return enableRowSelect ? (
          <div className={classes.rowExpandButton}>
            <UnexpandedIcon className='rowSelectionIcon' />
          </div>
        ) : null;
      },
      id: 'controls',
      isGutter: true,
      minWidth: 24,
      maxWidth: 24,
      width: 24,
      disableSortBy: true,
      disableFilters: true,
      disableResizing: true,
    });
  }

  return { columns, data };
}

// eslint-disable-next-line react-memo/require-memo
const DataTableImpl: React.FC<DataTableProps> = ({ table, ...options }) => {
  const classes = useDataTableStyles();

  // eslint-disable-next-line react-hooks/exhaustive-deps
  const { columns, data } = React.useMemo(() => decorateTable({ table, ...options }), [table]);

  // Begin: width math
  const [scrollbarContainer, setScrollbarContainer] = React.useState<HTMLElement>(null);
  const [header, setHeader] = React.useState<HTMLDivElement>(null);
  const scrollContainerRef = React.useCallback((el) => setScrollbarContainer(el), []);
  const headingsRef = React.useCallback((el) => setHeader(el), []);
  const { width: scrollbarWidth } = useScrollbarSize(scrollbarContainer);

  React.useEffect(() => {
    const handler = () => {
      if (header?.scrollLeft !== scrollbarContainer?.scrollLeft) {
        header?.scrollTo({ left: scrollbarContainer?.scrollLeft ?? 0 });
      }
    };
    handler();
    scrollbarContainer?.addEventListener('scroll', handler);
    return () => scrollbarContainer?.removeEventListener('scroll', handler);
  }, [scrollbarContainer, header]);

  // TODO(nick,PC-1050): When this changes, need to reflow current widths (try to keep ratios). Only if not resized?
  const { width: containerWidth, height: containerHeight } = React.useContext(AutoSizerContext);

  // Space not claimed by fixed-width columns is evenly distributed among remaining columns.
  const defaultWidth = React.useMemo(() => {
    const staticWidths = columns.map((c) => Number(c.width)).filter((c) => c > 0);
    const staticSum = staticWidths.reduce((a, c) => a + c, 0);
    return Math.floor((containerWidth - staticSum - scrollbarWidth) / (columns.length - staticWidths.length));
  }, [columns, containerWidth, scrollbarWidth]);

  const defaultColumn = React.useMemo(() => ({
    minWidth: 90,
    width: defaultWidth,
    maxWidth: 1800,
  }), [defaultWidth]);
  // End: width math

  // By default, we sort by the first column that has data (ascending, ignores control/gutter columns)
  const firstDataColumn = React.useMemo(
    () => columns?.find((col) => !!col.accessor && !col.isGutter),
    [columns]);

  const instance = useTable(
    {
      columns,
      data,
      defaultColumn,
      disableSortRemove: true,
      initialState: {
        sortBy: firstDataColumn ? [{ id: firstDataColumn.accessor as string }] : [],
      },
    },
    useBlockLayout, // Note: useFlexLayout would simplify a lot of the sizing code but it's buggy with useResizeColumns.
    useResizeColumns,
    useSortBy,
  );

  const [expanded, setExpanded] = React.useState<string>(null);
  const toggleRowExpanded = React.useCallback((rowId: string) => {
    if (expanded === rowId) setExpanded(null);
    else setExpanded(rowId);
  }, [expanded]);

  // Ensures an update when, for example, a quantiles column changes modes
  const colNames = React.useMemo(
    () => columns.map((c) => String(c.id ?? c.Header)).join(';'),
    [columns]);

  const ctx: DataTableContextProps = React.useMemo(() => ({
    instance,
    expanded,
    toggleRowExpanded,
    ...options,
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }), [
    // Monitoring specific parts of the instance ensures that the <List /> updates upon sort/resize/expand/etc events.
    instance, instance.totalColumnsWidth, instance.state.sortBy,
    options.enableRowSelect, options.enableColumnSelect,
    options.onRowSelected, options.onRowsRendered,
    colNames, expanded, toggleRowExpanded,
  ]);

  const ready = containerWidth > 0 && containerHeight > 0 && defaultWidth > 0;
  if (!ready) return null;

  return (
    <DataTableContext.Provider value={ctx}>
      <div role='table' className={classes.table} style={{ width: containerWidth, height: containerHeight }}>
        <HeaderRow ref={headingsRef} scrollbarWidth={scrollbarWidth} />
        <div
          className={classes.tableBody}
          role='rowgroup'
        >
          <List
            outerRef={scrollContainerRef}
            width={containerWidth}
            height={containerHeight - ROW_HEIGHT_PX}
            itemCount={instance.rows.length}
            itemSize={ROW_HEIGHT_PX}
            onItemsRendered={options.onRowsRendered}
            overscanCount={3}
          >
            {BodyRow}
          </List>
        </div>
      </div>
    </DataTableContext.Provider>
  );
};
DataTableImpl.displayName = 'DataTable';

export const DataTable = withAutoSizerContext(DataTableImpl);
