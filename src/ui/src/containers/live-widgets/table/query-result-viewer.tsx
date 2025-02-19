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

import { WidgetDisplay } from 'app/containers/live/vis';
import { Table } from 'app/api';
import * as React from 'react';
import { Theme, Typography, makeStyles } from '@material-ui/core';
import { createStyles } from '@material-ui/styles';
import { Arguments } from 'app/utils/args-utils';
import { LiveDataTable } from 'app/containers/live-data-table/live-data-table';

const useStyles = makeStyles(({ spacing }: Theme) => createStyles({
  root: {
    height: '100%',
    display: 'flex',
    flexDirection: 'column',
    overflowX: 'hidden',
  },
  table: {
    display: 'flex',
    flexGrow: 1,
  },
  tableSummary: {
    marginTop: spacing(1.5),
    marginBottom: spacing(0.5),
    paddingTop: spacing(1),
    paddingRight: spacing(1),
    textAlign: 'right',
  },
}), { name: 'QueryResultViewer' });

export interface QueryResultTableDisplay extends WidgetDisplay {
  gutterColumn?: string,
}

export interface QueryResultTableProps {
  display: QueryResultTableDisplay;
  data: Table;
  propagatedArgs: Arguments;
}

export const QueryResultTable: React.FC<QueryResultTableProps> = (({
  display, data, propagatedArgs,
}) => {
  const classes = useStyles();

  const [totalCount, setTotalCount] = React.useState<number>(0);

  const dataLength = data?.data?.length ?? 0;
  React.useEffect(() => {
    if (data && data.data) {
      setTotalCount(
        data.data.map((d) => d.getNumRows())
          .reduce((p, n) => p + n, 0));
    }
  }, [data, dataLength, setTotalCount]);

  const [visibleStart, setVisibleStart] = React.useState(1);
  const [visibleStop, setVisibleStop] = React.useState(1);
  const visibleRowSummary = React.useMemo(() => {
    const count = visibleStop - visibleStart + 1;
    let text = `Showing ${visibleStart + 1} - ${visibleStop + 1} / ${totalCount} records`;
    if (count <= 0) {
      text = 'No records to show';
    } else if (count >= totalCount) {
      text = '\xa0'; // non-breaking space
    }
    return <Typography variant='subtitle2'>{text}</Typography>;
  }, [totalCount, visibleStart, visibleStop]);

  const onRowsRendered = React.useCallback(({ visibleStartIndex, visibleStopIndex }) => {
    setVisibleStart(visibleStartIndex);
    setVisibleStop(visibleStopIndex);
  }, []);

  return (
    <div className={classes.root}>
      <div className={classes.table}>
        <LiveDataTable
          table={data}
          gutterColumn={display.gutterColumn}
          propagatedArgs={propagatedArgs}
          onRowsRendered={onRowsRendered}
        />
      </div>
      <div className={classes.tableSummary}>
        {visibleRowSummary}
      </div>
    </div>
  );
});
