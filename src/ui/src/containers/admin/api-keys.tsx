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

import {
  gql, useQuery, useMutation, useLazyQuery,
} from '@apollo/client';
import Table from '@material-ui/core/Table';
import TableBody from '@material-ui/core/TableBody';
import TableHead from '@material-ui/core/TableHead';
import TableRow from '@material-ui/core/TableRow';
import { distanceInWords } from 'date-fns';
import * as React from 'react';

import { GQLAPIKey, GQLAPIKeyMetadata } from 'app/types/schema';
import {
  AdminTooltip, StyledTableCell, StyledTableHeaderCell, StyledLeftTableCell,
} from './utils';
import {
  UseKeyListStyles, KeyActionButtons,
} from './key-list';
import { MonoSpaceCell } from './cluster-table-cells';

interface APIKeyDisplay {
  id: string;
  idShort: string;
  createdAt: string;
  desc: string;
}

export function formatAPIKey(apiKey: GQLAPIKeyMetadata): APIKeyDisplay {
  const now = new Date();
  return {
    id: apiKey.id,
    idShort: apiKey.id.split('-').pop(),
    createdAt: `${distanceInWords(new Date(apiKey.createdAtMs), now, { addSuffix: false })} ago`,
    desc: apiKey.desc,
  };
}

export const APIKeyRow: React.FC<{ apiKey: APIKeyDisplay }> = ({ apiKey }) => {
  const [deleteAPIKey] = useMutation<{ DeleteAPIKey: boolean }, { id: string }>(gql`
    mutation DeleteAPIKeyFromAdminPage($id: ID!) {
      DeleteAPIKey(id: $id)
    }
  `);

  const [copyKeyToClipboard] = useLazyQuery<{ apiKey: GQLAPIKey }>(gql`
    query getAPIKey($id:ID!){
      apiKey(id: $id) {
        id
        key
      }
    }`, {
    fetchPolicy: 'network-only',
    onCompleted: (data) => {
      navigator.clipboard.writeText(data?.apiKey?.key);
    },
  });

  return (
    <TableRow key={apiKey.id}>
      <AdminTooltip title={apiKey.id}>
        <StyledLeftTableCell>{apiKey.idShort}</StyledLeftTableCell>
      </AdminTooltip>
      <StyledTableCell>{apiKey.createdAt}</StyledTableCell>
      <StyledTableCell>{apiKey.desc}</StyledTableCell>
      <MonoSpaceCell data={'••••••••••••'} />
      <StyledTableCell>
        <KeyActionButtons
          copyOnClick={() => { copyKeyToClipboard({ variables: { id: apiKey.id } }); }}
          deleteOnClick={() => deleteAPIKey({
            variables: { id: apiKey.id },
            update: (cache, { data }) => {
              if (!data.DeleteAPIKey) {
                return;
              }
              cache.modify({
                fields: {
                  apiKeys(existingKeys, { readField }) {
                    return existingKeys.filter(
                      (key) => (apiKey.id !== readField('id', key)),
                    );
                  },
                },
              });
            },
            optimisticResponse: { DeleteAPIKey: true },
          })} />
      </StyledTableCell>
    </TableRow >
  );
};

export const APIKeysTable: React.FC = () => {
  const classes = UseKeyListStyles();

  const { data, error } = useQuery<{ apiKeys: GQLAPIKeyMetadata[] }>(
    gql`
      query getAPIKeysForAdminPage{
        apiKeys {
          id
          desc
          createdAtMs
        }
      }
    `,
    // As API keys can be sensitive, skip cache on first fetch.
    { pollInterval: 60000, fetchPolicy: 'network-only', nextFetchPolicy: 'cache-and-network' },
  );

  if (error) {
    return <div className={classes.error}>{error.toString()}</div>;
  }

  const apiKeys = (data?.apiKeys ?? []).map((key) => formatAPIKey(key));
  return (
    <>
      <Table>
        <TableHead>
          <TableRow>
            <StyledTableHeaderCell>ID</StyledTableHeaderCell>
            <StyledTableHeaderCell>Created</StyledTableHeaderCell>
            <StyledTableHeaderCell>Description</StyledTableHeaderCell>
            <StyledTableHeaderCell>Value</StyledTableHeaderCell>
            <StyledTableHeaderCell></StyledTableHeaderCell>
          </TableRow>
        </TableHead>
        <TableBody>
          {apiKeys.map((apiKey: APIKeyDisplay) => (
            <APIKeyRow key={apiKey.id} apiKey={apiKey} />
          ))}
        </TableBody>
      </Table>
    </>
  );
};
