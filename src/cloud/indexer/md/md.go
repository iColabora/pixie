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

package md

import (
	"context"
	"fmt"

	"github.com/gofrs/uuid"
	"github.com/olivere/elastic/v7"
	log "github.com/sirupsen/logrus"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/shared/services/msgbus"
)

// VizierIndexer run the indexer for a single vizier index.
type VizierIndexer struct {
	st       msgbus.Streamer
	es       *elastic.Client
	vizierID uuid.UUID
	orgID    uuid.UUID
	k8sUID   string

	sub    msgbus.PersistentSub
	quitCh chan bool
	errCh  chan error
}

// NewVizierIndexer creates a new Vizier indexer.
func NewVizierIndexer(vizierID uuid.UUID, orgID uuid.UUID, k8sUID string, st msgbus.Streamer, es *elastic.Client) *VizierIndexer {
	return &VizierIndexer{
		st:       st,
		es:       es,
		vizierID: vizierID,
		orgID:    orgID,
		k8sUID:   k8sUID,
		quitCh:   make(chan bool),
		errCh:    make(chan error),
	}
}

// Run starts the indexer.
func (v *VizierIndexer) Run(topic string) {
	log.
		WithField("VizierID", v.vizierID).
		WithField("ClusterUID", v.k8sUID).
		Info("Starting Indexer")

	sub, err := v.st.PersistentSubscribe(topic, "indexer"+IndexName, v.streamHandler)
	if err != nil {
		log.WithError(err).Error("Failed to subscribe")
	}
	v.sub = sub

	for {
		select {
		case <-v.quitCh:
			return
		case <-v.errCh:
			log.WithField("vizier", v.vizierID.String()).WithError(err).Error("Error during indexing")
		}
	}
}

// Stop stops the indexer.
func (v *VizierIndexer) Stop() {
	close(v.quitCh)
	err := v.sub.Close()
	if err != nil {
		log.WithError(err).Error("Failed to un-subscribe from channel")
	}
}

func (v *VizierIndexer) nsUpdateToEMD(u *metadatapb.ResourceUpdate, nsUpdate *metadatapb.NamespaceUpdate) *EsMDEntity {
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                nsUpdate.UID,
		Name:               nsUpdate.Name,
		NS:                 nsUpdate.Name,
		Kind:               "namespace",
		TimeStartedNS:      nsUpdate.StartTimestampNS,
		TimeStoppedNS:      nsUpdate.StopTimestampNS,
		RelatedEntityNames: []string{},
		UpdateVersion:      u.UpdateVersion,
		State:              getStateFromTimestamps(nsUpdate.StopTimestampNS),
	}
}

func podPhaseToState(podUpdate *metadatapb.PodUpdate) ESMDEntityState {
	switch podUpdate.Phase {
	case metadatapb.PENDING:
		return ESMDEntityStatePending
	case metadatapb.RUNNING:
		return ESMDEntityStateRunning
	case metadatapb.SUCCEEDED:
		return ESMDEntityStateTerminated
	case metadatapb.FAILED:
		return ESMDEntityStateFailed
	default:
		return ESMDEntityStateUnknown
	}
}

func getStateFromTimestamps(stopTimestamp int64) ESMDEntityState {
	if stopTimestamp > 0 {
		return ESMDEntityStateTerminated
	}
	return ESMDEntityStateRunning
}

func (v *VizierIndexer) podUpdateToEMD(u *metadatapb.ResourceUpdate, podUpdate *metadatapb.PodUpdate) *EsMDEntity {
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                podUpdate.UID,
		Name:               podUpdate.Name,
		NS:                 podUpdate.Namespace,
		Kind:               "pod",
		TimeStartedNS:      podUpdate.StartTimestampNS,
		TimeStoppedNS:      podUpdate.StopTimestampNS,
		RelatedEntityNames: []string{},
		UpdateVersion:      u.UpdateVersion,
		State:              podPhaseToState(podUpdate),
	}
}

func (v *VizierIndexer) serviceUpdateToEMD(u *metadatapb.ResourceUpdate, serviceUpdate *metadatapb.ServiceUpdate) *EsMDEntity {
	if serviceUpdate.PodIDs == nil {
		serviceUpdate.PodIDs = make([]string, 0)
	}
	return &EsMDEntity{
		OrgID:              v.orgID.String(),
		VizierID:           v.vizierID.String(),
		ClusterUID:         v.k8sUID,
		UID:                serviceUpdate.UID,
		Name:               serviceUpdate.Name,
		NS:                 serviceUpdate.Namespace,
		Kind:               "service",
		TimeStartedNS:      serviceUpdate.StartTimestampNS,
		TimeStoppedNS:      serviceUpdate.StopTimestampNS,
		RelatedEntityNames: serviceUpdate.PodIDs,
		UpdateVersion:      u.UpdateVersion,
		State:              getStateFromTimestamps(serviceUpdate.StopTimestampNS),
	}
}

func (v *VizierIndexer) resourceUpdateToEMD(update *metadatapb.ResourceUpdate) *EsMDEntity {
	switch update.Update.(type) {
	case *metadatapb.ResourceUpdate_NamespaceUpdate:
		return v.nsUpdateToEMD(update, update.GetNamespaceUpdate())
	case *metadatapb.ResourceUpdate_PodUpdate:
		return v.podUpdateToEMD(update, update.GetPodUpdate())
	case *metadatapb.ResourceUpdate_ServiceUpdate:
		return v.serviceUpdateToEMD(update, update.GetServiceUpdate())
	default:
		// We don't care about any other update types.
		// Notably containerUpdates and nodeUpdates.
		return nil
	}
}

const elasticUpdateScript = `
if (params.updateVersion <= ctx._source.updateVersion)  {
  ctx.op = 'noop';
}
ctx._source.relatedEntityNames.addAll(params.entities);
ctx._source.relatedEntityNames = ctx._source.relatedEntityNames.stream().distinct().sorted().collect(Collectors.toList());
ctx._source.timeStoppedNS = params.timeStoppedNS;
ctx._source.updateVersion = params.updateVersion;
ctx._source.state = params.state;
`

func (v *VizierIndexer) streamHandler(msg msgbus.Msg) {
	ru := metadatapb.ResourceUpdate{}
	err := ru.Unmarshal(msg.Data())
	if err != nil { // We received an invalid message through stan.
		log.WithError(err).Error("Could not unmarshal message from stan")
		v.errCh <- err
		err = msg.Ack()
		if err != nil {
			log.WithError(err).Error("Failed to ack stan msg")
		}
		return
	}

	err = v.HandleResourceUpdate(&ru)
	if err != nil {
		log.WithError(err).Error("Error handling resource update")
		v.errCh <- err
		err = msg.Ack()
		if err != nil {
			log.WithError(err).Error("Failed to ack stan msg")
		}

		return
	}

	err = msg.Ack()
	if err != nil {
		log.WithError(err).Error("Failed to ack stan msg")
	}
}

// HandleResourceUpdate indexes the resource update in elastic.
func (v *VizierIndexer) HandleResourceUpdate(update *metadatapb.ResourceUpdate) error {
	esEntity := v.resourceUpdateToEMD(update)
	if esEntity == nil { // We are not handling this resource yet.
		return nil
	}

	id := fmt.Sprintf("%s-%s-%s", v.vizierID, v.k8sUID, esEntity.UID)
	_, err := v.es.Update().
		Index(IndexName).
		Id(id).
		Script(
			elastic.NewScript(elasticUpdateScript).
				Param("entities", esEntity.RelatedEntityNames).
				Param("timeStoppedNS", esEntity.TimeStoppedNS).
				Param("updateVersion", esEntity.UpdateVersion).
				Param("state", esEntity.State).
				Lang("painless")).
		Upsert(esEntity).
		Refresh("true").
		Do(context.Background())
	return err
}
