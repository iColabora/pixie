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

package main

import (
	"net/http"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"k8s.io/client-go/rest"

	atpb "px.dev/pixie/src/cloud/artifact_tracker/artifacttrackerpb"
	"px.dev/pixie/src/cloud/config_manager/configmanagerpb"
	"px.dev/pixie/src/cloud/config_manager/controller"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/services/env"
	"px.dev/pixie/src/shared/services/healthz"
	"px.dev/pixie/src/shared/services/server"
	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	pflag.String("artifact_tracker_service", "kubernetes:///artifact-tracker-service.plc:50750", "The artifact tracker service url (load balancer/list is ok)")
	pflag.String("prod_sentry", "", "Key for prod Viziers that is used to send errors and stacktraces to Sentry.")
	pflag.String("dev_sentry", "", "Key for dev Viziers that is used to send errors and stacktraces to Sentry.")
}

func newArtifactTrackerClient() (atpb.ArtifactTrackerClient, error) {
	dialOpts, err := services.GetGRPCClientDialOpts()
	if err != nil {
		return nil, err
	}

	artifactTrackerChannel, err := grpc.Dial(viper.GetString("artifact_tracker_service"), dialOpts...)
	if err != nil {
		return nil, err
	}

	return atpb.NewArtifactTrackerClient(artifactTrackerChannel), nil
}

func main() {
	services.SetupService("config-manager-service", 50500)
	services.PostFlagSetupAndParse()
	services.CheckServiceFlags()
	services.SetupServiceLogging()

	mux := http.NewServeMux()
	// This handles all the pprof endpoints.
	mux.Handle("/debug/", http.DefaultServeMux)
	healthz.RegisterDefaultChecks(mux)

	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Fatal("Unable to get incluster kubeconfig")
	}
	clientset := k8s.GetClientset(kubeConfig)

	atClient, err := newArtifactTrackerClient()
	if err != nil {
		log.WithError(err).Fatal("Could not connect with Artifact Service.")
	}
	svr := controller.NewServer(atClient, clientset)
	serverOpts := &server.GRPCServerOptions{
		DisableAuth: map[string]bool{
			"/px.services.ConfigManagerService/GetConfigForVizier": true,
		},
	}
	s := server.NewPLServerWithOptions(env.New(viper.GetString("domain_name")), mux, serverOpts)
	configmanagerpb.RegisterConfigManagerServiceServer(s.GRPCServer(), svr)
	s.Start()
	s.StopOnInterrupt()
}
